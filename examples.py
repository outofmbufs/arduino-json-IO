#!/usr/bin/env python

#
# Just some python code showing one way to access the JSON APIs
#
# DEPENDENCIES: needs the "requests" module: pip install requests
#

import requests
import json

# CHANGE THIS to match your Arduino's IP addr

server="http://192.168.62.118"

# reading a single pin:
def read_one_pin(pinnumber):
  url = server + '/v1/analogRead/' + str(pinnumber)
  response = requests.request('GET', url)
  print_response(response)

# printing a response from the server
def print_response(response):
  if response.status_code == 200:          # HTTP OK
    python_dictionary = response.json()
    print(python_dictionary)
  else:
    print("Server returned error")

#
# Given those functions, now read and display pin:
#
read_one_pin(3)

# the same thing, reading multiple pins in a single request using POST
#  Specify pinlist as an array, e.g., [ 1, 2, 3 ]
def read_multiple_pins(pinlist):
  url = server + '/v1/analogRead'
  # some extra steps - need to encode the parameters accordingly:
  params = { "pins" : pinlist }
  JSONparams = json.dumps(params)

  # this header is not actually required by the Arduino, but you get
  # the good housekeeping seal of approval if you send it, since we ARE
  # sending json we should declare it as such in the request
  JSONhdr = { 'Content-Type' : 'application/json' }

  response = requests.request('POST', url, headers=JSONhdr, data=JSONparams)
  print_response(response)


# try the multiple pin thing
read_multiple_pins([ 1, 2, 3 ])

# setting a pin to output mode
url = server + '/v1/configure/pinmode'
params = { 'modes' : { 'pin' : 7, 'mode' : 'OUTPUT' } }
JSONparams = json.dumps(params)

resp = requests.request('POST', url, 
                        headers={'Content-Type' : 'application/json'},
                        data=JSONparams)

# write a HIGH to that pin
params = { 'writes' : { 'pin' : 7, 'value' : 'HIGH' } }
JSONparams = json.dumps(params)
url = server + '/v1/digitalWrite'
resp = requests.request('POST', url, 
                        headers={'Content-Type' : 'application/json'},
                        data=JSONparams)

# that's the essence of it ... have fun!

