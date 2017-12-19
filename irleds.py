import requests
import json


def sendcodes(url, irinfo, repn=None):
    """ POST irinfo dictionaries to the server. Return HTTP result.

    irinfo should be an array of ircode dictionaries (it can be a naked
    dictionary if you only have one). You must specify "bits" and "protocol"
    in the first dictionary but you can elide those in the subsequent ircodes.
    The last values specified will carry forward (within this single request).
    The "delay" parameter similarly carries forward but also has a built-in
    server default so often does not need to be given.
    """
    d = {"codes": irinfo}
    if repn is not None:
        d["repeat"] = repn
    return requests.post(url, data=json.dumps(d, separators=(',', ':')))

def busypin(server, pin):
    d = {"modes" : {"pin" : pin, "mode" : "BUSY" }}
    return requests.post(server+"v1/configure/pinmode", data=json.dumps(d))

def makeNEC(*codes):
    """Return array of ircodes suitable for using in sendcodes.

    Each codes arg should either be a simple code, or a tuple: (code, delay)
    """
    a = []
    for x in codes:
        try:
            a.append({'code': x[0], 'delay': x[1]})
        except TypeError:
            a.append({'code': x})
    return [{'protocol': 'NEC', 'bits': 32, **a[0]}] + a[1:]


class LEDControls:
    """The actual IR codes (all are NEC/32)."""
    BRIGHTER = 16726725
    DIMMER = 16759365
    PLAYSTOP = 16745085
    POWER = 16712445
    RED = 16718565
    # four buttons descending under the RED button
    RED1 = 16722645
    ORANGE2 = 16714485
    ORANGE3 = 16726215
    YELLOW4 = 16718055
    R1 = RED1
    R2 = ORANGE2
    R3 = ORANGE3
    R4 = YELLOW4

    GREEN = 16751205
    GREEN1 = 16755285
    GREEN2 = 16747125
    BLUE3 = 16758855
    BLUE4 = 16750695
    G1 = GREEN1
    G2 = GREEN2
    G3 = BLUE3
    G4 = BLUE4

    BLUE = 16753245
    BLUE1 = 16749165
    PURPLE2 = 16757325
    PURPLE3 = 16742535
    PINK4 = 16734375
    B1 = BLUE1
    B2 = PURPLE2
    B3 = PURPLE3
    B4 = PINK4

    WHITE = 16720605
    PINK1 = 16716525
    PINK2 = 16724685
    WBLUE3 = 16775175
    WBLUE4 = 16767015
    W1 = PINK1
    W2 = PINK2
    W3 = WBLUE3
    W4 = WBLUE4

    RED_UP = 16722135
    RED_DOWN = 16713975

    GREEN_UP = 16754775
    GREEN_DOWN = 16746615

    BLUE_UP = 16738455
    BLUE_DOWN = 16730295

    QUICK = 16771095
    SLOW = 16762935

    DIY1 = 16724175
    DIY2 = 16756815
    DIY3 = 16740495
    DIY4 = 16716015
    DIY5 = 16748655
    DIY6 = 16732335

    AUTO = 16773135
    FLASH = 16764975

    JUMP3 = 16720095
    JUMP7 = 16752735
    FADE3 = 16736415
    FADE7 = 16769055

