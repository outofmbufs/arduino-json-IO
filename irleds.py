import requests
import json

#
# See LICENSE file for license (MIT license)
#
# Interface to the Arduino JSON server for the IR emitter.
#
# The server accepts POSTs to /v1/sendIR like this:
#
#     { "codes": code-dicts, "repeat": nnn }
#
# Where:
#   code-dicts is either a single code-dictionary or an
#              array of such dictionaries.
#
#   repeat is optional integer, and causes the server to behave
#          as-if this had happened:
#
#              for i in range(nnn):
#                  POST { "codes": code-dicts }
#
# A code-dictionary is:
#   { "code": irvalue, "bits": bbb, "protocol": protocol-name, delay: usecs }
#
# Where:
#   irvalue is the numeric IR value to be transmitted (application specific)
#   bbb is the number of bits to transmit (protocol specific)
#   protocol-name is a string like "NEC" (see server code for supported names)
#   usecs is optional and is the delay, in microseconds, imposed AFTER sending
#
# When sending multiple code-dicts (i.e, an array of them), values
# for "bits", "protocol" and "delay" will carry forward from the first
# code-dictionary in the array to each subsequent once IN THE SINGLE REQUEST.
# This allows for smaller POST requests, e.g.:
#   { "codes": [ {"code": 123, "bits": 32, "protocol": "NEC"},
#                {"code": 456 }, {"code": 789} ] }
#
# is a compact way to send 123 then 456 then 789 using NEC/32 protocol.
#


def sendcodes(server, code_dicts, repn=0):
    """POST code dictionaries to the server. Return HTTP result."""
    d = {"codes": code_dicts}
    if repn > 0:
        d["repeat"] = repn
    s = json.dumps(d, separators=(',', ':'))  # squeeze out spaces
    if server == "debug":
        print("Would send: [{}]\n".format(s))
    else:
        requests.post(server+"/v1/sendIR", data=s)


def busypin(server, pin):
    """Configure the given pin to be the "BUSY" indicator pin."""
    outputpin(server, pin, "BUSY")


def outputpin(server, pin, mode="OUTPUT"):
    d = {"modes": {"pin": pin, "mode": mode}}
    if server == "debug":
        print("Configuring pin {} as {}".format(pin, mode))
    else:
        requests.post(server+"/v1/configure/pinmode", data=json.dumps(d))
    

def makeNEC(*codes):
    """Return array of ircodes suitable for using in sendcodes.

    Each codes arg should either be:
       - a simple code
       - or a tuple: (code, delay)
       - or a full-on dict { "code": code, ... }
    """
    a = [{'protocol': 'NEC', 'bits': 32}]
    for x in codes:
        try:
            # if x is a tuple, it should be (code, delay)
            a.append({'code': x[0], 'delay': x[1]})
        except KeyError:
            # x is a dictionary, use it as is
            a.append(x)
        except TypeError:
            # x is (or should be) just a naked code
            a.append({'code': x})
    return a
