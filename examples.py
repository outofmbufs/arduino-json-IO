#!/usr/bin/env python

#
# Just some python code showing one way to access the JSON APIs
#
# DEPENDENCIES: needs the "requests" module: pip install requests
#

import requests
import json

# CHANGE THIS to match your Arduino's IP addr

server="http://192.168.99.99"

# a whole bunch of simple routines demonstrating one way to access the APIs
# These all use the global "server" variable and are really just meant
# as demonstrations / explanation rather than as examples of good coding :)
#

# reading a single pin:
def read_one_pin(pinnumber):
  url = server + '/v1/analogRead/' + str(pinnumber)
  response = requests.request('GET', url)
  for pv in response.json()['pins']:
    if pv['pin'] == pinnumber:
      return pv['value']

# reading multiple pins ... pinlist should be [ p1, p2, ... ]
#
def read_npins(pinlist):
  url = server + '/v1/analogRead'
  params = { "pins" : pinlist }
  JSONparams = json.dumps(params)
  JSONhdr = { 'Content-Type' : 'application/json' }
  response = requests.request('POST', url, headers=JSONhdr, data=JSONparams)
  # just return the results list
  return response.json()['pins']

# writing a single pin
def write_one_pin(pinnumber, what):
  url = server + '/v1/digitalWrite'
  params = { 'writes' : { 'pin' : pinnumber, 'value' : what } }
  JSONparams = json.dumps(params)
  JSONhdr = { 'Content-Type' : 'application/json' }
  resp = requests.request('POST', url, headers=JSONhdr, data=JSONparams)

# writing multiple pins ...
# pinsvals should be an array of tuples:
#     [ (p, v), (p, v), ... ]
# where each p is a pin number and each v is a value to be written
#
# Values can be 'HIGH' or 'LOW' (case matters!) or a number
#
def write_npins(pinsvals):
  url = server + '/v1/digitalWrite'
  params = { 'writes' : [ {'pin' : p, 'value' : v} for p,v in pinsvals ] }
  JSONparams = json.dumps(params)
  JSONhdr = { 'Content-Type' : 'application/json' }
  resp = requests.request('POST', url, headers=JSONhdr, data=JSONparams)

# configure a particular pin as an output
def pin_output_mode(pin):
  url = server + '/v1/configure/pinmode'
  params = { 'modes' : { 'pin' : pin, 'mode' : 'OUTPUT' } }
  JSONparams = json.dumps(params)
  JSONhdr = { 'Content-Type' : 'application/json' }
  resp = requests.request('POST', url, headers=JSONhdr, data=JSONparams)

def requests_processed():
  url = server + '/v1/status'
  response = requests.request('GET', url)
  return response.json()['requestsProcessed']

def server_uptime():
  url = server + '/v1/status'
  response = requests.request('GET', url)
  return response.json()['uptime_msecs']//1000

#
# Given those functions, now read and display pin:
#
print("Pin 1 is {}".format(read_one_pin(1)))

# read/display multiple pins:
for pv in read_npins([1, 2, 3]):
    print("Pin {} is {}".format(pv['pin'],pv['value']))

# do some configuration and writes...
pin_output_mode(5)
pin_output_mode(6)
write_npins([ (5, 'HIGH'), (6, 'HIGH') ])
write_one_pin(6, 'LOW')
print("Served {} requests".format(requests_processed()))

# that's the essence of it ... have fun!

