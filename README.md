# analogjson

An arduino web server with JSON interface to the digital and analog IO functions of an Arduino.

The idea behind this is you can access the full functionality of the analogRead/digitalRead/digitalWrite etc functions of the Arduino via network operations (assuming you have an Ethernet shield of course). And, of course, it provides a framework for showing you how to implement other simple JSON HTTP APIs.

Licensed under the MIT license for maximum flexibility; see the LICENSE file for details.


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

```
     { "pins" : [ { "pin" : nn, "value" : vv } ... ] }
```
    where the number of `{ "pin" : nn, "value" : vv }` elements in the array will correspond to the number of pin readings you requested (one for the simple GET form; possibly N for the JSON POST form of request).


* **GET /v1/digitalRead/{PINNUMBER}** ... same as analogRead but digital.

* **POST /v1/digitalRead** ... same as analogRead but digital.

* **POST /v1/digitalWrite**

    Writes any number of pins (digitalWrite). You post JSON that looks like:
```
     { "writes" : [ { "pin" : nn, "value" : vv } ... ] }
```
    and the given values are written to the given pins. Values can be the literals `HIGH` or `LOW` (case SENSITIVE), or numbers. By repeating pin/value pairs inside the array you can write multiple pins with one HTTP transaction (they are, of course, still multiple API calls inside the Arduino).

    No data is returned (just HTTP 200 OK).

* **POST /v1/configure/pinmode** 

    lets you call pinMode on a given pin. The POSTed data should look like:

```
     { "modes" : [ { "pin" : nn, "mode" : mm } ... ] }

    the mode values can be numbers or the literals INPUT, OUTPUT, or INPUT_PULLUP. Multiple pins can be configured in a single HTTP transaction. Be mindful of the overall size limits on JSON requests/responses.

    No data is returned (just HTTP 200 OK).

* **GET /v1/status** Returns a JSON status object with various information.


## Acknowledgements / Other Licensing

Includes the JSMN JSON parser written by Serge A. Zaitsev. The files `jsmn.c` and `jsmn.h` were obtained from [http://zserge.com/jsmn.html](http://zserge.com/jsmn.html) with the license reproduced here:

```
Copyright (c) 2010 Serge A. Zaitsev

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

```
