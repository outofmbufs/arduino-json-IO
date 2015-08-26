# analogjson

An arduino web server with JSON interface to the digital and analog IO functions of an Arduino.

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

    There is an arbitrary compiled-in limit on the number of pins you can read in one operation (by default this limit is 10). Please note that increasing this limit may also have effects on other buffer sizes chosen in the program.

* [ more interfaces still to be implemented TBD ]

When reading one or more pins, the server returns JSON that looks like this:

```
     { "pins" : [ { "pin" : nn, "value" : vv } ... ] }
```

where the number of `{ "pin" : nn, "value" : vv }` elements in the array will correspond to the number of pin readings you requested (one for the simple GET form; possibly N for the JSON POST form of request).

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
