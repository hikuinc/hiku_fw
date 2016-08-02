from queue import Queue
from threading import Thread 
import time
import sys

import requests
import os
from pathlib import Path 
import logging

from functools import partial

import datetime
import json
import sys, os, base64, hashlib, hmac 

logging.basicConfig(level=logging.DEBUG, format='%(threadName)s - %(asctime)s - %(message)s')
logging.getLogger('requests').setLevel(logging.CRITICAL)
logger = logging.getLogger(__name__)


class AudioUploadThread(Thread):

    def __init__(self, queue):
        #super(AudioUploadThread, self).__init__()
        Thread.__init__(self)
        self.queue = queue

    def wavToRawData(self, data):
        print ('testing wavToRawData')

    def run(self):
    	valid = True

    	while valid: 
	        sequence, audio, valid = self.queue.get()
	        #print ('Uploading', audio)

	        
	        payload = {'sequence': sequence, 'valid': valid, 'size': sys.getsizeof(audio)}

	       	logger.info('seq={}, valid={}, chunksize={}'.format(sequence, valid, sys.getsizeof(audio)))

	       	#if valid: 
	        r = requests.put('https://agent.electricimp.com/bDpYGqlCeeC_/play', data=audio, params=payload)
	        print (r.text)
	        print ('sending http request to agent')

	        self.queue.task_done()

	    #print('Exiting Upload Thread')


class TextToSpeechThread(Thread):

    def __init__(self, textdata = '', texttype = '', rate = '', voicename = '', voicelanguage = '', voicegender = ''):
        Thread.__init__(self)

        self.textdata = textdata
        self.texttype = texttype
        self.rate = rate
        self.voicename = voicename
        self.voicelanguage = voicelanguage
        self.voicegender = voicegender

        self.method = "POST"
        self.accessKey="GDNAJSTKA6OI3XF7CMQQ"
        self.secretKey="aXit6NSwXsbNRBycPLoDdpJxQ/m0kstL73UEY49K"

        self.content_type = "application/json"
        self.host = "tts.us-west-2.ivonacloud.com"
        self.canonical_uri = "/CreateSpeech"
        self.canonical_querystring = ''

        self.signed_headers = 'content-type;host;x-amz-content-sha256;x-amz-date'
        self.service = 'tts'
        self.region = 'us-west-2'
        self.algorithm = 'AWS4-HMAC-SHA256'


    def sign(self, key, msg):
        return hmac.new(key, msg.encode('utf-8'), hashlib.sha256).digest()

    def getSignatureKey(self, key, date_stamp, regionName, serviceName):
        kDate = self.sign(('AWS4' + key).encode('utf-8'), date_stamp)
        kRegion = self.sign(kDate, regionName)
        kService = self.sign(kRegion, serviceName   )
        kSigning = self.sign(kService, 'aws4_request')
        return kSigning


    def jsonifyRequestParameters(self, text = '', rate = '', voice = '', language = '', gender = ''):
        # Create a hash for the jsonified request content
        d = {
            'Input' : {
                'Data' : "Hello world. This is haiku.",
                'Type' : "application/ssml+xml",
            }
        }            
        return json.dumps(d)


    def sign_request(self):

        # Task 1: CREATE A CANONICAL REQUEST *************
        # create a date for headers and the credential string
        t = datetime.datetime.utcnow()
        amz_date = t.strftime('%Y%m%dT%H%M%SZ')    #UTC time format - used in canonical header
        date_stamp = t.strftime('%Y%m%d')          # Date w/o time - used in credential scope

        # jsonify the request parameters and generate the hash to send as part of canonical request
        request_parameters = self.jsonifyRequestParameters()
        payload_hash = hashlib.sha256(request_parameters.encode('utf-8')).hexdigest()
 
        # create canonical headers and then a request 
        canonical_headers = 'content-type:' + self.content_type + '\n' + 'host:' + self.host + '\n' + 'x-amz-content-sha256:' + payload_hash + '\n' + 'x-amz-date:' + amz_date + '\n'
        canonical_request = self.method + '\n' + self.canonical_uri + '\n' + self.canonical_querystring + '\n' + canonical_headers + '\n' + self.signed_headers + '\n' + payload_hash
        print ('\n' + 'canonical_request = ' + '\n' + canonical_request)

        
        # Task 2: CREATE THE STRING TO SIGN *************
        credential_scope = date_stamp + '/' + self.region + '/' + self.service + '/' + 'aws4_request'
        string_to_sign = self.algorithm + '\n' +  amz_date + '\n' +  credential_scope + '\n' +  hashlib.sha256(canonical_request.encode('utf-8')).hexdigest()
        print ('\n' + 'string_to_sign = ' + '\n' + string_to_sign)


        # Task 3: CALCULATE THE SIGNATURE *************
        # Create the signing key using the function defined above.
        signing_key = self.getSignatureKey(self.secretKey, date_stamp, self.region, self.service)
        # Sign the string_to_sign using the signing_key
        signature = hmac.new(signing_key, (string_to_sign).encode('utf-8'), hashlib.sha256).hexdigest()
        print ('\n' + 'signature = ' + '\n' + signature)


        # Task 4: ADD SIGNING INFORMATION TO THE REQUEST *************
        # Put the signature information in a header named Authorization.
        authorization_header = self.algorithm + ' ' + 'Credential=' + self.accessKey + '/' + credential_scope + ', ' +  'SignedHeaders=' + self.signed_headers + ', ' + 'Signature=' + signature
        print ('\n' + 'authorization_header = ' + '\n' + authorization_header)


        # Task 5: COMPLETE SIGNED REQUEST *************
        headers = {
                    'Content-Type':self.content_type,
                   'X-Amz-Date':amz_date,
                   'Authorization':authorization_header, 
                   'x-amz-content-sha256': payload_hash,
                   'Content-Length': len(request_parameters)
                   }
        endpoint = 'https://{}{}'.format(self.host, self.canonical_uri)

        # ************* SEND THE REQUEST *************
        print ('\n' + 'BEGIN REQUEST++++++++++++++++++++++++++++++++++++')
        print ('Request URL = ' + endpoint)

        r = requests.post(endpoint, data=request_parameters, headers=headers)

        print ('\n' + 'RESPONSE++++++++++++++++++++++++++++++++++++')
        print ('Response code: %d\n' % r.status_code)
 
    def run(self):
        self.sign_request()

        # ************* SEND THE REQUEST *************
        # The 'host' header is added automatically by the Python 'request' lib. But it
        # must exist as a header in the request.
        #request_url = endpoint + "?" + canonical_querystring

        #print '\nBEGIN REQUEST++++++++++++++++++++++++++++++++++++'
        #print 'Request URL = ' + request_url
        #r = requests.get(request_url)

        #print '\nRESPONSE++++++++++++++++++++++++++++++++++++'
        #print 'Response code: %d\n' % r.status_code
        #print r.text



def main():
    seq = 0

    ts = time.time()

    queue = Queue()

    '''for x in range(1):
    	worker = AudioUploadThread(queue)
    	#worker.daemon = True		#look into this later
    	worker.start()

    with open('amy5.wav', 'rb') as f:
        #data = f.read()
    	records = iter(partial(f.read, 5000), b'')
    	for chunk in records:
    		seq+=1
    		queue.put((seq, chunk, True))
    '''

    worker = TextToSpeechThread()
    worker.start()
    worker.join()

    '''seq+=1
    #queue.put((seq, '', False))
    queue.join()
    '''

    print ('Took {}'.format(time.time() - ts))

if __name__ == '__main__':
   main()
