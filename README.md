# analogjson

An arduino web server with JSON interface to the digital and analog IO functions of an Arduino.

The idea behind this is you can access the full functionality of the analogRead/digitalRead/digitalWrite etc functions of the Arduino via network operations (assuming you have an Ethernet shield of course). And, of course, it provides a framework for showing you how to implement other simple JSON HTTP APIs.

Licensed under the MIT license for maximum flexibility; see the LICENSE file for details.

## Infrared Remote Support

The code has been extended with the ability to send IR codes using the IRRemote library. You can conditionally build the program with just the IR support, just the analog/digital I/O support, or both. You may instead wish to take this code as an example or starting point and fully customize it for your own application as well.

## Development Status
I have tested most of the code but have not written or executed exhaustive tests of every feature. I'm using this code in my own projects and find it useful; your mileage may vary!

## APIs implemented

All endpoints start with /v1 to indicate version number (1) of this protocol.

* **GET /v1/analogRead/{PINNUMBER}**

    Performs an analogRead on pin {PINNUMBER} and returns a JSON "pins" list as described below.

* **POST /v1/analogRead**

    Reads any number of pins and returns a JSON "pins" list as described below. The POST data must be a JSON object with this form:

    `{ "pins" : nn }`

    which will read a single pin as indicated by `nn`, or:

    `{ "pins" : [ nn, mm, ... ] }`

    which will read multiple pins as indicated by `nn`, `mm`, and so forth.

    Returns a JSON "pins" list as described below.

    There is an arbitrary compiled-in limit on the number of pins you can read in one operation.

    When reading one or more pins, the server returns JSON that looks like this:

    `{ "pins" : [ { "pin" : nn, "value" : vv } ... ] }`

    where the number of `{ "pin" : nn, "value" : vv }` elements in the array will correspond to the number of pin readings you requested (one for the simple GET form; possibly N for the JSON POST form of request).


* **GET /v1/digitalRead/{PINNUMBER}** ... same as analogRead but digital.

* **POST /v1/digitalRead** ... same as analogRead but digital.

* **POST /v1/digitalWrite**

    Writes any number of pins (digitalWrite). You post JSON that looks like:

    `{ "writes" : [ { "pin" : nn, "value" : vv } ... ] }`

    and the given values are written to the given pins. Values can be the literals `HIGH` or `LOW` (case SENSITIVE), or numbers. By repeating pin/value pairs inside the array you can write multiple pins with one HTTP transaction (they are, of course, still multiple API calls inside the Arduino).

    No data is returned (just HTTP 200 OK).

* **POST /v1/configure/pinmode**

    lets you call pinMode on a given pin. The POSTed data should look like:

    `{ "modes" : [ { "pin" : nn, "mode" : mm } ... ] }`

    the mode values can be numbers or the literals INPUT, OUTPUT, or INPUT_PULLUP. Multiple pins can be configured in a single HTTP transaction. Be mindful of the overall size limits on JSON requests/responses.

    A pin can also be set to BUSY mode. Setting a pin to BUSY configures it as an OUTPUT that will be driven HIGH during request processing and LOW otherwise. This is mostly useful with IRSend as a way to see activity that might not otherwise be apparent (assuming you hook something up to the BUSY pin of course).

    A successful configuration request returns HTTP 200 OK.

* **GET /v1/status** Returns a JSON status object with various information.

* **POST /v1/sendIR**

    Sends an arbitrary IR code on the default output pin (usually pin 3) of the IRSend library. The POSTed data should look like:

    `{ "codes" : {code-dict} }`

    or

    `{ "codes" : [ {code-dict1}, {code-dict2}, ... ] }`

    and can optionally include a repeat element:

    `{ "codes" : {code-dict}, "repeat": 4 }`

    `{ "codes" : [ ... ], "repeat": 4 }`


    Each code-dict looks something like:

    `{ "code": 16712445, "bits": 32, "protocol": "NEC", "delay": 250000 }`

    where the "code", "bits", and "protocol" are determined by what type of IR code you want to send and are passed accordingly to the IRSend library. The "delay" element sets the amount of time, in microseconds, the server will sleep after sending the code. This is useful for separating consecutive POST operations according to whatever device-specific timing requirements there may be. The default "delay" is 150000 (150 milliseconds). You can set delay zero if you want no delay at all but be aware that devices may get confused by codes sent too quickly back-to-back.

    If you send multiple code-dicts in a single request, the "bits", "protocol" and "delay" elements will default to whatever value was found in any previous code-dict IN THIS SAME POST REQUEST. This is helpful for reducing the POST size when sending multiple codes all using the same protocol (e.g., NEC/32) and delay parameters.

    Finally, the "repeat" element at the top level will cause the server to loop over the entire POST request that many times. So, for example, if you have a code that increases brightness and you want the brightness up to the max and you know that sending the code eight times is always enough to get to the max you can specify the brightness IR code once and use `"repeat": 8` to send it 8 times (you may need to specify an appropriate delay in the code-dict for this to work properly). Be careful with this; obviously very long repeat cycles will make the Arduino web server unresponsive for the duration of all the sends.

## Using a Browser

As a bonus side-effect of having some simple GET APIs, you can surf your arduino with any browser to read a pin. If your arduino has IP address 192.168.99.99, then type this URL into a browser:

    http://192.168.99.99/v1/analogRead/1

and you'll see something like:

    {"serverVersion":"20150826.1","pins":[{"pin":1,"value":392}]}

which isn't pretty, but does tell you that pin 1 currently has value 392. Unfortunately there's no easy way to POST arbitrary data from a browser so you'll have to write code or use `curl` or something along those lines to write to your pins. But you can surf them from any browser at any time using this server.

You can also get device status:

    http://192.168.99.99/v1/status

which will show something like:

    {"serverVersion":"20150826.1","requestsProcessed":217,"uptime_msecs":54001313}

## Acknowledgements / Other Licensing

Includes the JSMN JSON parser written by Serge A. Zaitsev. The files `jsmn.c` and `jsmn.h` were obtained from [http://zserge.com/jsmn.html](http://zserge.com/jsmn.html) with the license reproduced here:


>Copyright (c) 2010 Serge A. Zaitsev
>
>Permission is hereby granted, free of charge, to any person obtaining a copy
>of this software and associated documentation files (the "Software"), to deal
>in the Software without restriction, including without limitation the rights
>to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
>copies of the Software, and to permit persons to whom the Software is
>furnished to do so, subject to the following conditions:
>
>The above copyright notice and this permission notice shall be included in
>all copies or substantial portions of the Software.
>
>THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
>IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
>FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
>AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
>LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
>OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
>THE SOFTWARE.
>
