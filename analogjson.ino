//
// Copyright (c) 2015 Neil Webber.
//
// See LICENSE file for licensing terms.
//

#include <SPI.h>
#include <Ethernet.h>

#include "jsmn.h"     // JSON parser

// A NOTE ABOUT STRINGS
//
// There are many string literals in here. They could (should?) be moved
// into program memory and accessed via PROGMEM hacks. But that makes
// the code ugly. As this is mostly for example purposes, I've chosen to
// leave them be. Everything fits within 2K still at the moment.
//


// ---------------------
// Arbitrary Size Limits
// ---------------------
//
// There are many arbitrary limits compiled in here; you need to pick these
// (modify them) according to your application requirements. For embedded
// things I prefer the simplicity of this vs a full-on malloc attack.
//
// You need to set the REQUESTBUFFERSIZE to be large enough for the larger of:
//      -- the biggest URL you are going to use
//      -- the biggest JSON data you are going to exchange. This is
//         usually the bigger requirement; if you want to send multiple
//         pins in a single request the syntax adds up fast.
//
// Similarly, you need to set the MAXJSMNTOKENS to be large enough so that
// the JSMN parser can parse whatever JSON you send.
//
// In both cases if something comes in that is too large it will just be
// discarded and an error returned to the client.
//
// These default constants allow most requests to have a few pins specified.
//

#ifndef REQUESTBUFFERSIZE
#define REQUESTBUFFERSIZE 200
#endif

#ifndef MAXJSMNTOKENS
#define MAXJSMNTOKENS 30
#endif

// The MAC address this Arduino will use
//
// Arduino Ethernet shields come with an address; mine were labeled
// on the down side of the board.  Use that (edit it in here) if you can.
//
// Otherwise, make one up like this. If you are making it up then the first
// byte MUST be even and it SHOULD NOT be a multiple of 4 (official ones
// are multiples of 4).  See, e.g., "MAC_address" on wikipedia.
//
byte mac[] = { 0x66, 0x66, 0x65, 0x11, 0x22, 0x34 };

// web server on port 80
EthernetServer server(80);

// version string
const char revision[] = "20150826.1";

// this counter is for statistics; reported via status API
unsigned long requests_processed = 0;


// compute elapsed time (in milliseconds) from time t0
// Takes millis() wrap-around into account, though obviously
// it cannot return the correct answer for time spans greater than
// the total wrap-around time of approximately 50 days.

unsigned long
elapsed_time(unsigned long t0)
{
    unsigned long t1 = millis();
    if (t1 >= t0)
        return t1 - t0;
    else {
        //
        // t1 is "before" t0, which presumably means the timer has
        // wrapped around (happens after about 50 days of up time).
        //
        // I assume there is some simpler/more-clever way to do this, but:
        // Basically we need to add 0x100000000 to the incorrectly "lower"
        // time t1 before doing the subtraction, but of course we need
        // to do this carefully to stay within 32 bits to make it work.
        //

        // we want (t1 + 0x100000000) - t0 and get there this way:
        //     0xFFFFFFFF - t0
        //        +t1             (will not overflow because we know t1<t0)
        //        +1

        unsigned long tmp = 0xFFFFFFFF;
        tmp -= t0;     // 0xFFFFFFFF - t0
        tmp += t1;     // (0xFFFFFFFF - t0) + t1

        tmp &= 0xFFFFFFFF;  // make sure it works on 64 bit machines (haha)
        return tmp + 1; // the rest of the 0x100000000 we needed to add
    }
}

// ---------------
//  JSMN helpers
// ---------------
//
// The JSMN "parser" is really just a JSON tokenizer.
// These routines help with (specialized/targeted) parsing of JSON trees
// from the tokenized output of JSMN.
//

// String compare:
//   tp is a jsmn token pointing into (start/stop indices into) buf.
//   Compare those characters against "k" with strcmp semantics.
//
int
jsmn_strcmp(const jsmntok_t *tp, const char *k, const char *buf)
{
    return strncmp(k, &buf[tp->start], tp->end - tp->start);
}

// Integer conversion:
//   tp is a jsmn token pointing into (start/stop indices into) buf
//   Convert it to an integer, stopping at the first non-digit character.
//
// Returns 0 if nothing converted, which of course is indistinguishable
// from an actual 0 converted (if you care about the distinction you need
// to do more validation before calling this)

int
jsmn_atoi(const jsmntok_t *tp, const char *buf)
{
    int rv = 0;
    int sign = 1;

    for (int i = tp->start; i < tp->end; i++) {
        char c = buf[i];

        if (isdigit(c)) {
            rv *= 10;
            rv += (c - '0');
        } else if (i > tp->start) {
            // non-digit and not first character
            break;
        } else {
            // non-digit and first character, allow '+' or '-'
            if (c == '-')
                sign = -1;
            else if (c != '+')    // i.e., ignore '+', fail all others
                break;
        }

    }

    rv *= sign;
    return rv;
}


// Token skip:
//
// You've got a token list that starts with a token you aren't interested in.
// This skips forward over that token and everything it recursively contains.
//
// Returns the number of tokens skipped, which by definition is always
// at least one (the one you initially point us at)
//
// It's up to you to know whether this was your last token (i.e., if
// by skipping it you've reached the end of the token list)
//

int
jsmn_skiptok(const jsmntok_t *toks)
{
    int nskipped = 1;   // i.e. we already know we're skipping THIS one
    int n;

    // for JSMN_PRIMITIVE or JSMN_STRING, by definition we're only
    // skipping one token, so there's nothing else to do.
    //
    // For the ARRAY or OBJECT containers, iterate through their contents
    // and recursively skip them.

    if (toks->type == JSMN_ARRAY) {
	for (n = toks->size; n > 0; n--)
            nskipped += jsmn_skiptok(toks+nskipped);
    } else if (toks->type == JSMN_OBJECT) {
        // Subtle: the key is always just one token (an object key cannot
        // be an array or another object). So we can skip that one with
        // just a +1, and then recursively skip the value of the key
        // (positioning to that is the other +1)
	for (n = toks->size; n > 0; n--)
            nskipped += 1 + jsmn_skiptok(toks+nskipped+1); // key and value
    }

    return nskipped;
}

// Extract a key value from an object
//
//    objtp points to the start of a JSMN_OBJECT in a token list
//    buf is the character buffer for the tokens
//    k is the key you seek
//
// Return the index (relative to objtp) of the value if found, or -1
//

int
jsmn_findkeyval(jsmntok_t *objtp, const char *buf, const char *k)
{
    int nskipped = 1;

    if (objtp->type == JSMN_OBJECT) {
	for (int n = objtp->size; n > 0; n--) {
            jsmntok_t *tp = objtp+nskipped;

            if (jsmn_strcmp(tp, k, buf) == 0)
		return nskipped+1;
            else
                nskipped += 1 + jsmn_skiptok(tp+1);   // skip key and value
	}
    }
    return -1;
}

// -----------------------
// HTTP/Ethernet utilities
// -----------------------


// eatline - consume characters until LF
//
// Strictly speaking we should be looking for CRLF but
// this works for all properly formatted requests
//
// Returns -1 if there was a problem (timeout, connection lost, etc)
//

int
eatline(EthernetClient &ec, unsigned long t0)
{
    for(;;) {
        int i;

        if ((i = clientgetc(ec, t0)) == -1)
            return -1;
        if (i == '\n')
            return 0;
     }
}

// skipwhite - consume white space characters
//
// Returns the first non-space character or -1 for timeouts/disconnect/etc

int
skipwhite(EthernetClient &ec, unsigned long t0)
{
    for(;;) {
        int i;

        if ((i = clientgetc(ec, t0)) == -1)
            return -1;
        if (! isspace((char)i))
            return i;
     }
}

// get a character
//
// Get a character from the client, with timeout and disconnect checks.
// Returns the character, or -1 if unsuccessful.
//
// To protect against clients that connect and send nothing this bails
// out after "too long". No "real" browser ever causes this but you can
// test it by telnetting to port 80, entering at least one character and
// then just waiting.
//

#define HTTPPARSE_TIMEOUT_SECONDS    20L
#define HTTPPARSE_TIMEOUT            (HTTPPARSE_TIMEOUT_SECONDS * 1000L)

int
clientgetc(EthernetClient &ec, unsigned long t0)
{
    // NOTE that this is in fact a busy loop, but in practice there should
    //      already be data when we are here. In any case it's not like we
    //      have anything else to do and the TIMEOUT eventually applies.

    while ((elapsed_time(t0) <= HTTPPARSE_TIMEOUT) && ec.connected()) {
        if (ec.available())
            return ec.read();
    }
    return -1;
}

// get n characters from the client
// buf must be n+1 long (NUL added, which may or may not make sense but
// it is always added at the end nevertheless)
int
clientgetn(EthernetClient &ec, char *buf, int n, unsigned long t0)
{
    for ( ; n > 0 ; --n) {
        int i = clientgetc(ec, t0);

        if (i == -1)
            return -1;

        *buf++ = (char)i;
    }
    *buf = '\0';
    return 0;
}

// ------------
// HTTP Parsing
// ------------
//
// First a little request structure to hold results and context:
//     client         -- the EthernetClient we are working with
//     t0             -- start time; used for timeouts
//     bufp           -- buffer to hold the resulting URL
//     bufsiz         -- how big bufp is
//     content_length -- if a Content-Length: header was given, the
//                       value supplied; else -1.
//     operation      -- HTTPPARSE_GET or HTTPPARSE_POST

#define HTTPPARSE_NONE               0
#define HTTPPARSE_GET                1
#define HTTPPARSE_POST               2

struct http_rq {
    EthernetClient &client;
    unsigned long t0;         // used for enforcing timeouts
    int  content_length;
    int  operation;
    char *bufp;
    int bufsiz;
    http_rq(EthernetClient &ec) : client(ec) {}  // reference requires init
};

// ------------------
// URL function table
// ------------------
//
// This is instantiated later (after all the function definitions)
// This table connects particular URLs, operations, and their implementation
//

struct urlfuncs {
    char *url;
    int whatop;
    void (* f)(struct http_rq *, struct urlfuncs *);
};






// Parsing the first line of an HTTP request
// By RFC, it should look like:
//
//       [VERB][SP][URL][SP]HTTP/1.1[CRLF]
//
// where:
//    [VERB] is "GET" or "POST" etc and is CASE SENSITIVE (per RFC)
//    [SP]   is literally and EXACTLY ONE space character (per RFC)
//    [URL]  is the /blah/foo/frammis part
//    [CRLF] is literally/exactly '\r\n'
//
// The RFC is very specific and strict on this format, and we are not
// very forgiving here in the interests of keeping a small code footprint.
//
// Returns zero for success.
//

int
parsefirstline(struct http_rq *rp)
{
    char *p = NULL;
    char c;
    int i;

    // get the very first character, which since we only understand
    // GET and POST is enough to distinguish those cases. If we ever
    // add PUT requests this will need some minor rework.

    if ((i = clientgetc(rp->client, rp->t0)) == -1)
        return -1;
    c = (char)i;
    if (c == 'G') {
        rp->operation = HTTPPARSE_GET;
        p = "ET";
    } else if (c == 'P') {
        // if PUT becomes an option, add more logic here; for now...
        rp->operation = HTTPPARSE_POST;
        p = "OST";
    } else
        return -1;

    // at this point we are looking for the rest of GET/POST (or disconnected)
    while (*p != '\0') {
        if ((i = clientgetc(rp->client, rp->t0)) == -1)
            return -1;
        c = (char)i;
        if (c != *p++)        // i.e., not matching expected GET or POST
            return -1;
    }

    // next character must be a space per RFC
    if (clientgetc(rp->client, rp->t0) != ' ')
        return -1;

    // at this point we matched the verb and have read the space. Yay!
    // What comes next should be the url; put it into the buffer.
    // NOTE: It's up to you to define your protocol endpoints to fit in
    // the buffer size you also defined. We check for overflow; overflow
    // causes the request to be discarded. Choose your compile-time
    // constants/sizes according to your application requirements.

    p = rp->bufp;

    for(;;) {   // go until hit ' ' or timeout or buffer limit
        if ((i = clientgetc(rp->client, rp->t0)) == -1)
            return -1;
        c = (char)i;
        if (c == ' ') {          // end of the URL
            *p = '\0';           // note we ensured room for this (bufsiz-1)
            break;
        }
        if (p == rp->bufp+(rp->bufsiz-1))   // always leave room for '\0'
            return -1;            // hit end of buffer before end of URL
        *p++ = c;                 // still have room; copy this char
    }

    // have the URL so now consume the HTTP/1.1 part. We don't check it.
    return eatline(rp->client, rp->t0);
}

// Parsing Content-Length
//
// This is called whenever a header field starts with 'C', and if
// the header field does in fact match Content-Length: then we
// parse it and store the length value.
//
// Note that header fields are cAse iNSensiTiVe
//
// All other headers starting with C are ignored (as are all other headers)
//
int
tryCLen(struct http_rq *rp)
{
    const char *p = "ONTENT-LENGTH:";
    int i;
    char c;

    for(;;) {
        if ((i = clientgetc(rp->client, rp->t0)) == -1)
            return -1;
        c = (char)i;

        if (c == ':' && *p == ':')
            break;                         // woohoo, found it!

        if (isalpha(c))
            c = toupper(c);

        if (c != *p++)                   // nope, not content-length
            return eatline(rp->client, rp->t0);
    }

    // we have Content-Length: ... now there is arbitrary white space
    if ((i = skipwhite(rp->client, rp->t0)) == -1)
        return -1;
    c = (char)i;

    // convert the content-length field, note we already have the first char
    rp->content_length = (c - '0');
    for (;;) {
        if ((i = clientgetc(rp->client, rp->t0)) == -1)
            return -1;
        c = (char)i;
        if (isdigit(c)) {
            rp->content_length *= 10;
            rp->content_length += (c - '0');
        } else if (c == '\n')         // no CR? really shouldn't happen
            return 0;
        else
            return eatline(rp->client, rp->t0);
    }
}

//
// The top level for all this parsing mess.
// Returns 0 for success, -1 for error.
//
// Not fully RFC-compliant parsing; we have compromised for footprint.
// Does not enforce the requirement for a Host: header (which we'd ignore
// so we don't even look for it; this permissiveness violates the RFC).
//
// On return, the Ethernet client stream will have been read through the
// blank line (\r\n) terminating the headers (i.e., the next byte should be
// the first byte of the POST data if there is any)
//
// any string not fitting within bufsiz is just truncated
// Make your buffer big enough for the GET string length you care about
//
int
parseHTTPheader(struct http_rq *rp)
{
    int i;
    char c;

    rp->operation = HTTPPARSE_NONE;
    rp->content_length = -1;

    if (parsefirstline(rp) == -1)
        return -1;

    // so, the deal is we need to consume all the headers.
    // we are only checking for one very specific header: Content-Length.
    // If that is encountered we'll fill in the rp->content_length field.
    // Everything else is just consumed/ignored.

    int linelen = 0;    // will track as we go; a zero length line ends headers
    for(;;) {
        if ((i = clientgetc(rp->client, rp->t0)) == -1)
            return -1;
        c = (char)i;
        if (isalpha(c))
            c = toupper(c);

        if (c == '\n') {
            if (linelen == 0)
                return 0;         // got a blank line
            linelen = 0;
        } else if (c != '\r') {   // not conformant, but sufficient: ignore CR
            ++linelen;
            if (c == 'C' && linelen == 1) {
                // headers starting with 'C' are further inspected...
                if (tryCLen(rp) == -1)
                    return -1;
                linelen = 0;
            }
        }
    }
}

// basic successful reply (in some cases followed by JSON data)
const char httpreply1[] = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n";

// basic unsuccessful reply
const char reply400[] =
    "HTTP/1.1 400 Bad Request\r\n\Connection: close\r\n\r\n";


// reading pins and producing JSON
//
// This is the guts of analogRead and digitalRead, both for a single
// pin (GET) or for multiple pins (POSTed request). The readerfunc
// is either analogRead or digitalRead as appropriate. The jsmn token pointer
// is pointing to the list (possibly just one) of json pin read "objects"
//
// Do the reads and send JSON that looks like
//
// { "pins" : [ { "pin" : nn, "value" : vv } ], ... }
//
// Note that there are other elements in this object (e.g., serverVersion)
// so the client must JSON parse the returned object accordingly.
//
// The above general format is used even if there is just one pin to report.
// The pin/value object is always inside an array inside "pins".
//
void
processjsonreads(EthernetClient &ec,
                 jsmntok_t *tp,
                 char *bufp,
                 int (*readerfunc)(uint8_t))
{
    int n;

    if (tp->type == JSMN_STRING || tp->type == JSMN_PRIMITIVE) // just "3" etc
        n = 1;
    else if (tp->type == JSMN_ARRAY) {  // [ 1, 3, { "something bogus" : 1 } ]
        n = tp->size;
        tp++;    // skip to first contained object
    }

    // blurt out the preamble...
    ec.print(httpreply1);
    ec.print("{\"serverVersion\":\"");
    ec.print(revision);
    ec.print("\",\"pins\":[");

    // walk through the JSON, reading and blurting more out as we go
    int needcomma = 0;
    for ( ; n > 0; n--) {
        if (tp->type == JSMN_STRING || tp->type == JSMN_PRIMITIVE) {
            int pin = jsmn_atoi(tp, bufp);

            if (needcomma)
                ec.print(",");
            ec.print("{\"pin\":");
            ec.print(pin);
            ec.print(",\"value\":");
            // We certainly could try to enforce some sanity on pinnum,
            // but instead "you get whatever happens when you ask for it"
            ec.print((* readerfunc)(pin));  // analogRead or digitalRead
            ec.print("}");
            needcomma = 1;
        }
        tp += jsmn_skiptok(tp);
    }
    ec.print("]}");
}



// GET handler for analog or digital reading
//
// Reads a single pin as specified in the tail of the URL.
//
// Skip the leading part (as defined by the urlfunc entry) and process
// the pin number, take it as gospel. No sanity is enforced, you get what
// you ask for whether that makes sense or not.
//
void
do_one_read(struct http_rq *rp, struct urlfuncs *ufp, int (*rf)(uint8_t))
{
    // we just fake up a JSON request... a little hokey but here we go
    jsmntok_t token;

    token.type = JSMN_STRING;
    token.start = strlen(ufp->url);
    token.end = strlen(rp->bufp);
    token.size = 0;      // a STRING "contains" no other elements

    processjsonreads(rp->client, &token, rp->bufp, rf);
}

void
do_one_Aread(struct http_rq *rp, struct urlfuncs *ufp)
{
    do_one_read(rp, ufp, analogRead);
}

void
do_one_Dread(struct http_rq *rp, struct urlfuncs *ufp)
{
    do_one_read(rp, ufp, digitalRead);
}




// POST handler for analog/digital reading
//
// Reads multiple pins as specified in the POSTed JSON which
// will either be a single pin:
//               { "pins" : nn }
//
// or multiple:  { "pins" : [ nn, mm, xx, yy ] }
//
// No sanity is enforced on pin numbers, you get what you ask for.
//
// Response will look like:
// { "pins" : [ { "pin" : nn, "value" : vv }, ... ], ... }
//
// and the client must parse the JSON accordingly (there are other elements
// in the returned object, e.g., serverVersion).
//
void
do_json_reads(struct http_rq *rp, struct urlfuncs *ufp, int (*rf)(uint8_t))
{
    int i;
    int n;
    jsmn_parser parser;
    jsmntok_t tokens[MAXJSMNTOKENS];

    // we're done with the URL itself so note that we're re-using
    // that buffer for the JSON in the POST now. Yay small footprints :)
    if (rp->content_length > rp->bufsiz-1 ||
        clientgetn(rp->client, rp->bufp, rp->content_length, rp->t0) == -1) {

        rp->client.print(reply400);
        return;
    }

    jsmn_init(&parser);
    n = jsmn_parse(&parser, rp->bufp, strlen(rp->bufp), tokens, MAXJSMNTOKENS);
    i = ((n > 0) ? jsmn_findkeyval(tokens, rp->bufp, "pins") : -1);

    if (i == -1) {
        // didn't find it
        rp->client.print(reply400);
        return;
    }

    // i is the value token index
    processjsonreads(rp->client, &tokens[i], rp->bufp, rf);
}

void
do_json_Areads(struct http_rq *rp, struct urlfuncs *ufp)
{
    do_json_reads(rp, ufp, analogRead);
}

void
do_json_Dreads(struct http_rq *rp, struct urlfuncs *ufp)
{
    do_json_reads(rp, ufp, digitalRead);
}

// GET handler for status
//
// report on various status items
// Response is a simple JSON object of various key/value items
//
void
do_status(struct http_rq *rp, struct urlfuncs *ufp)
{
    rp->client.print(httpreply1);
    rp->client.print("{\"serverVersion\":\"");
    rp->client.print(revision);
    rp->client.print("\",\"requestsProcessed\":");
    rp->client.print(requests_processed);
    rp->client.print(",\"uptime_msecs\":");
    rp->client.print(millis());
    rp->client.print("}");
}

// This is a somewhat-generic outline for handling several similar
// types of POST requests.
//
// The theory here is that we have an object or an array that looks
// like this, as an example using the pinmode API:
//
//             { "modes" : { "pin" : pp, "mode" : mm } }
//        or   { "modes" : [ { "pin" : pp, "mode" : mm }, ... ] }
//
// But the digitalWrite API uses the same general outline, except
// that instead of "pinmode" it says "digitalwrite" and instead of
// "mode" it says "value", and the list of allowed substitutions
// (e.g., HIGH/LOW) is different. And, of course, the function to
// be called is different.
//
// So this is a generic routine that handles all that. I could have
// provided a call-specific substitution table (e.g., "OUTPUT" vs "HIGH")
// but instead we just allow all the substitutions on any API even though
// the "wrong" ones probably don't make sense.
//

struct pinop_params {
    char *topobj;           // e.g., "modes"
    void (* callback)(int, int, struct pinop_params *);
    void *callback_data;
    char *tokbuf;
    char *key1;              // e.g., "pin"
    char *key2;              // e.g., "mode" or "value"
};


int
pinops(struct pinop_params *parms)
{
    int n;
    int i;
    jsmn_parser parser;
    jsmntok_t tokens[MAXJSMNTOKENS];
    jsmntok_t *tp = NULL;
    char *bufp = parms->tokbuf;   // just a shorthand

    jsmn_init(&parser);
    n = jsmn_parse(&parser, bufp, strlen(bufp), tokens, MAXJSMNTOKENS);
    if (n < 1 || (i = jsmn_findkeyval(tokens, bufp, parms->topobj)) == -1)
        return -1;

    tp = &tokens[i];
    if (tp->type == JSMN_ARRAY) {
        n = tp->size;                  // number of contained settings
        tp++;                          // skip array itself (i.e., go to first)
    } else if (tp->type == JSMN_OBJECT) {
        n = 1;                         // directly specified object
    } else {
        return -1;                     // you have screwed up
    }

    for (; n > 0; n--) {
        int ix1 = jsmn_findkeyval(tp, parms->tokbuf, parms->key1);
        int ix2 = jsmn_findkeyval(tp, parms->tokbuf, parms->key2);

        // we're just silently ignoring entries without necessary fields
        if ((ix1 != -1) && (ix2 != -1)) {
            int v1 = jsmn_atoi(&tp[ix1], parms->tokbuf);
            int v2;

            jsmntok_t *tpm = &tp[ix2];
            if (tpm->type == JSMN_STRING) {
                if (jsmn_strcmp(tpm, "OUTPUT", parms->tokbuf) == 0)
                    v2 = OUTPUT;
                else if (jsmn_strcmp(tpm, "INPUT", parms->tokbuf) == 0)
                    v2 = INPUT;
                else if (jsmn_strcmp(tpm, "INPUT_PULLUP", parms->tokbuf) == 0)
                    v2 = INPUT_PULLUP;
                else if (jsmn_strcmp(tpm, "HIGH", parms->tokbuf) == 0)
                    v2 = HIGH;
                else if (jsmn_strcmp(tpm, "LOW", parms->tokbuf) == 0)
                    v2 = LOW;
                else
                    v2 = jsmn_atoi(tpm, parms->tokbuf);
            } else {
                v2 = jsmn_atoi(tpm, parms->tokbuf);
            }
            (* parms->callback)(v1, v2, parms);
            tp += jsmn_skiptok(tp);
        }
    }
    return 0;
}

// the callback for pinmode interface
void
call_pinmode(int pin, int mode, struct pinop_params *ignored)
{
    pinMode(pin, mode);
}

// POST handler for configuring pinmodes
//
// Instructions for calling pinMode are in the POST:
//
//     { "modes" : { "pin" : pp, "mode" : mm } }
// or
//     { "modes" : [ { "pin" : pp, "mode" : mm }, ... ] }
//
// The second form allows multiple pinmodes per single HTTP operation
//
// We accept numbers for mode or: "OUTPUT", "INPUT", "INPUT_PULLUP"
//
// Return is just an empty 200 / OK
//
void
do_pinmode(struct http_rq *rp, struct urlfuncs *ufp)
{
    // we're done with the URL itself so note that we're re-using
    // that buffer for the JSON in the POST now. Yay small footprints :)
    if (rp->content_length > rp->bufsiz-1 ||
        clientgetn(rp->client, rp->bufp, rp->content_length, rp->t0) == -1) {
        rp->client.print(reply400);
        return;
    }

    struct pinop_params px;
    px.topobj = "modes";
    px.callback = call_pinmode;
    px.callback_data = NULL;
    px.tokbuf = rp->bufp;
    px.key1 = "pin";
    px.key2 = "mode";

    if (pinops(&px) == 0)
        rp->client.print(httpreply1);
    else
        rp->client.print(reply400);
}

// the callback for digitalWrite interface
void
call_digwrite(int pin, int val, struct pinop_params *ignored)
{
    digitalWrite(pin, val);
}

// POST handler for digitalWrites
//
// Instructions for calling digitalWrite are in the POST:
//
//     { "writes" : { "pin" : pp, "value" : mm } }
// or
//     { "writes" : [ { "pin" : pp, "value" : mm }, ... ] }
//
// The second form allows multiple pinmodes per single HTTP operation
//
// We accept numbers for values or: "HIGH", "LOW"
//
// Return is just an empty 200 / OK



void
do_digitalwrite(struct http_rq *rp, struct urlfuncs *ufp)
{
    // we're done with the URL itself so note that we're re-using
    // that buffer for the JSON in the POST now. Yay small footprints :)
    if (rp->content_length > rp->bufsiz-1 ||
        clientgetn(rp->client, rp->bufp, rp->content_length, rp->t0) == -1) {
        rp->client.print(reply400);
        return;
    }

    struct pinop_params px;
    px.topobj = "writes";
    px.callback = call_digwrite;
    px.callback_data = NULL;
    px.tokbuf = rp->bufp;
    px.key1 = "pin";
    px.key2 = "value";

    if (pinops(&px) == 0)
        rp->client.print(httpreply1);
    else
        rp->client.print(reply400);
}

void
send_badrequest(struct http_rq *rp, struct urlfuncs *ufp)
{
    rp->client.print(reply400);
}

// -----------
// URL scheme
// -----------
//
// This table defines the URL scheme and connects them to handlers.
// Obviously you could extend this (or trim it!)
//
// GET /v1/analogRead/NNNN
//    Returns (as JSON) an analogRead() of pin NNNN.
//
// POST /v1/analogRead
//    Performs an analogRead on one or more pins specified via JSON
//
// GET /v1/digitalRead/NNNN
//    Returns (as JSON) a digitalRead() of pin NNNN.
//
// POST /v1/digitalRead
//    Performs a digitalRead on one or more pins specified via JSON
//
// POST /v1/digitalWrite
//    Performs a digitalWrite on one or more pins specified via JSON
//
// POST /v1/configure/pinmode
//    Configure pins via pinMode() from POSTed JSON info
//
// GET /v1/status
//    Return miscellaneous debug/status info object
//
// Note that the trailing slash on some URLs is not an accident or
// superfluous; that tells the dispatch matcher whether or not the
// URL is a "complete" URL (nothing added at end) or whether there
// will be extra operation-specific stuff (in the URL) after that point.
//
struct urlfuncs dt[] =
      { { "/v1/analogRead/", HTTPPARSE_GET, do_one_Aread },
        { "/v1/analogRead", HTTPPARSE_POST, do_json_Areads },
        { "/v1/digitalRead/", HTTPPARSE_GET, do_one_Dread },
        { "/v1/digitalRead", HTTPPARSE_POST, do_json_Dreads },
        { "/v1/digitalWrite", HTTPPARSE_POST, do_digitalwrite },
        { "/v1/configure/pinmode", HTTPPARSE_POST, do_pinmode },
        { "/v1/status", HTTPPARSE_GET, do_status },
        { NULL, HTTPPARSE_NONE, send_badrequest }};


//
// called by the main loop to check and process web requests periodically
//
void
webProcessing(EthernetClient& ec)
{
    //
    //
    // As is often the case with a small embedded device, we aren't
    // necessarily fully parsing everything in a strictly conformant manner.
    // Everything works if you do things correctly; if your client sends
    // incorrectly formatted data the results are undefined (hopefully we
    // won't crash, but there's no guarantee all format errors are acted upon)
    //

    char sbuf[REQUESTBUFFERSIZE];     // XXX maybe should malloc
    struct http_rq rq(ec);

    rq.bufp = sbuf;
    rq.bufsiz = REQUESTBUFFERSIZE;
    rq.t0 = millis();                // time zero for enforcing timeouts

    if (parseHTTPheader(&rq) != -1) {
        struct urlfuncs *dtp;

        for (dtp = dt; dtp->url; dtp++) {
            if (dtp->whatop == rq.operation) {

                // if the URL pattern ends with '/' we only look at leading
                // equivalence (strncmp). This allows for extra info after
                // the endpoint. Otherwise the endpoint must match exactly.
            	int n = strlen(dtp->url);
            	if (dtp->url[n-1] == '/') {
            	    if (strncmp(dtp->url, sbuf, n) == 0)
            	        break;
            	} else {
            	    if (strcmp(dtp->url, sbuf) == 0)
            	        break;
                }
            }
        }
        (* dtp->f)(&rq, dtp);
        ++requests_processed;
    }
}



// the setup routine runs once when you press reset:
void setup()
{
    Ethernet.begin(mac);
    server.begin();
}

//
// the loop routine is called over and over again
//

void loop()
{
    // DHCP lease check/renewal (library only sends request if expired)
    Ethernet.maintain();

    // check for web requests and do them
    EthernetClient client = server.available();
    if (client) {
       webProcessing(client);
       client.stop();
    }
}
