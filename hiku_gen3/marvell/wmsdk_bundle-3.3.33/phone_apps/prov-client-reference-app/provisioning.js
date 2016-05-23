var http = require("http");
var async = require('async');
var utils = require('./utils.js');
var fs = require('fs');
eval(fs.readFileSync('curve255.js')+'');


/* All the global data structures required */
aes_key = new Array(4);
session_id = 0;
var padding = "0000";
var nw = new Object();
var random_bytes
var len;
var step = 1;

my_private_key = new Array(16);
var my_private_key1 = new Array(16);
var my_public_key = new Array(16);
var device_pub_key = new Array(16);
var random_number = new Array(64);
random_number_str = "";
var my_public_key_str = "";

/*------------------------------/prov/secure-session POST handler---------------------------------*/
/* This handler establishes a secure-session with the device.
   A call to below API does the following:
   1. Computes the "shared secret" using Curve25519 cryptography which is AES CTR 128-bit
   encryption/decryption key used for subsequent requests
   2. Stores the valid "session-identifier" which is used in following http GET/POST requests
*/
exports.secure_session = function secure_session(callback) {
    console.log("--------------------------STEP " + step + "-----------------------------");
    step++;
    console.log("Initiating Secure Session Request...");
    /* Generate random number */
    for (i = 0; i < 64; i++) {
	random_number[i] = utils.randomUInt8();
    }
    /* Convert the random number into hexadecimal string (format used while sending the data) */
    len = random_number[0].toString(16).length;
    if (len != 2) {
	random_number_str = padding.slice(0, 2 - len);
    }
    random_number_str = random_number_str + random_number[0].toString(16);
    for (i = 1; i < 64; i++) {
	len = random_number[i].toString(16).length;
	if (len != 2) {
	    random_number_str = random_number_str + padding.slice(0, 2 - len);
	}
	random_number_str = random_number_str + random_number[i].toString(16);
    }

    /* Generate Private Key */
    for (i = 0; i < 16; i++) {
	my_private_key[i] = utils.randomUInt16();
    }

    /* Generate the Public Key */
    my_public_key = curve25519(my_private_key);

    /* Converting array into hexadecimal string (format used while sending the data).
       Also, performing Endianess change since the "my_public_key" array elements are
       16-bit Big-Endian format.
       Note: The Endianess change is an optional step and specific to the platform/programming
       language being used */
    var j = 0;
    for (i = 0; i < 16; i++) {
	var val  = my_public_key[i].toString(16);
	/* Converting Big-Endian to Little-Endian */
	val = val.replace(/^(.(..)*)$/, "0$1");
	var a = val.match(/../g);
	a.reverse();
	var s2 = a.join("");
	my_public_key_str += utils.pad(s2, 4);
	j++;
    }

    nw["client_pub_key"] = my_public_key_str;
    nw["random_sig"] = random_number_str;

    /* HTTP POST request structure for /prov/secure-session */
    var options_secure_session = {
	hostname: '192.168.10.1',
	port: 80,
	path: '/prov/secure-session',
	method: 'POST',
	headers: {
	    'Content-Type':'application/x-www-form-urlencoded',
	    'Accept': '*/*',
	}
    };

    var data = "";
    console.log("Request: HTTP POST /prov/secure-session");
    console.log(JSON.stringify(nw, null, 2));
    options_secure_session.headers['Content-Length'] = JSON.stringify(nw).length;
    options_secure_session.method = 'POST';
    var req = http.request(options_secure_session, function(res) {
	res.setEncoding('utf8');
	res.on('data', function (chunk) {
	    data += chunk;
	});

	res.on('end', function(e) {
	    console.log("Response: " + JSON.stringify(JSON.parse(data), null, 2));
            /* Generate the shared secret from the response data received from the client */
            utils.generate_shared_secret(data, callback);
	});

    });
    req.on('error', function(e) {
	callback(1, {eflag:0, msg:"Error in sending http POST"});
    });
    req.write(JSON.stringify(nw));
    req.end();
}

/*------------------------------/prov/scan GET handler---------------------------------*/
/* This handler retrieves the scan results from the device. The data received from the
   device will be encrypted, so this handler decrypts the data using AES-CTR 128-bit and
   validates the SHA512 checksum. Please refer to the "Developer Guide" for more details */
var options_scan = {
    hostname: '192.168.10.1',
    port: 80,
    path: '/prov/scan',
    method: 'GET',
    headers: {
	'Accept': '*/*',
    }
};

var len = 0;
var json_network_data = new Object();
var passphrase = 'alpha123'
exports.scan = function scan(callback) {
    var scan_buffer = "";
    console.log("Sending HTTP GET request on /prov/scan...");
    options_scan.method = 'GET';
    options_scan.path = '/prov/scan?session_id=' + session_id;
    var req = http.request(options_scan, function(res) {
	res.setEncoding('utf8');
	res.on('data', function (chunk) {
	    scan_buffer += chunk.toString();
	});

	res.on('end', function(e) {
	    var json_scan = JSON.parse(scan_buffer);
	    var scan_results = "";
	    /* Decryption the encrypted scan results data and verify */
	    scan_results = utils.decrypt_secure_data(scan_buffer);
	    if (scan_results !== null) {
		console.log("/prov/scan response : " + scan_results);
		callback(null, {eflag:1, msg:""});
	    } else {
		callback(1, {eflag:1, msg:"Error in http GET /prov/scan"});
	    }
	});

    });

    req.on('error', function(e) {
	console.log('\nProblem GET with request: ' + e.message);
	callback(1, {eflag:1, msg:"Error in sending http GET /prov/scan"});
    });

    req.end();
};

/*------------------------------/prov/network POST handler---------------------------------*/
/* This handler constructs the http POST request having home network settings to the device.
   The data sent to the device will be encrypted, so this handler shall encrypt the network data
   using AES-CTR 128-bit and also send SHA512 checksum of un-encrypted data for verification.
   Please refer to the "Developer Guide" for more details */
var options_network = {
    hostname: '192.168.10.1',
    port: 80,
    path: '/prov/network',
    method: 'POST',
    headers: {
	'Content-Type':'application/x-www-form-urlencoded',
	'Accept': '*/*',
    }
};

network_data = new Object();

exports.network = function network(callback) {
    console.log("--------------------------STEP " + step + "-----------------------------");
    step++;
    console.log("Configuring home network...");
    console.log("Unencrypted Request Data : " + JSON.stringify(network_data, null, 2));
    /* Encrypting the home network data */
    var enc_json = utils.encrypt_secure_data(JSON.stringify(network_data));
    options_network.headers['Content-Length'] = JSON.stringify(enc_json).length;
    options_network.method = 'POST';
    /* Including the session-identifier (received as a part of /prov/secure-session request) in URL */
    options_network.path = '/prov/network?session_id=' + session_id;
    console.log("Request: HTTP POST " + options_network.path);
    console.log(JSON.stringify(enc_json, null, 2));
    var req = http.request(options_network, function(res) {
	  res.setEncoding('utf8');
	  res.on('data', function (chunk) {
	  console.log("Response: " + JSON.stringify(JSON.parse(chunk), null, 2));
		var nw_status_results = "";
		/* Decrypt the /prov/network response */
	  nw_status_results = utils.decrypt_secure_data(chunk);
		  if (nw_status_results !== null) {
		      var json_nw_results = JSON.parse(nw_status_results);
		      console.log("Decrypted Response: " + JSON.stringify(json_nw_results, null, 2));
		  } else {
          console.log("Could not decrypt the /prov/network response data");
          console.log('Bad request');
          callback(1, {eflag:0, msg:"Failure : /prov/network handler failed"});
      }
	    if (json_nw_results.success === 0) {
		    console.log("Success : Network POST successful");
		    callback(null, {eflag:1, msg:""});
	    } else {
		    console.log('Bad request');
		    callback(1, {eflag:0, msg:"Failure : /prov/network handler failed"});
	    }
	});
    });

    req.on('error', function(e) {
	emit.log('\nProblem POST with request: ' + e.message);
	callback(1, {eflag:0, msg:"Error in sending http POST /prov/network"});
    });
    req.write(JSON.stringify(enc_json));
    req.end();
};

/*---------------------------------/prov/net-info GET/POST handler----------------------------------*/
/* This handler checks the status of network connection and constructs the http POST request for client
   acknowledgement. The data sent/received to/from the device shall be encrypted/decrypted.
   Please refer to the "Developer Guide" for more details */
var options_client_ack = {
    hostname: '192.168.10.1',
    port: 80,
    path: '/prov/net-info',
    method: 'POST',
    headers: {
	'Content-Type':'application/x-www-form-urlencoded',
	'Accept': '*/*',
    }
};

var json_client_ack = '{"prov_client_ack":1}'

var options_sys = {
    hostname: '192.168.10.1',
    port: 80,
    path: '/prov/net-info',
    method: 'GET',
    headers: {
	'Accept': '*/*',
    }
};

var attempts = 0;
var substep = 0
exports.net_info = function net_info(callback) {
    console.log("--------------------------STEP " + step + "(" + substep + ")-----------------------------");
    step++;
    console.log("Checking network connection status...");
    setTimeout(function() {
  	var sys_results = "";
  	options_sys.method = 'GET';
  	options_sys.path = '/prov/net-info?session_id=' + session_id;
	console.log("Request: HTTP GET" + options_sys.path);
  	var req = http.request(options_sys, function(res) {
	    res.setEncoding('utf8');
	    res.on('data', function (chunk) {
		sys_results += chunk
	    });

	    res.on('end', function(e) {
		var prov_results = "";
		console.log("Response: " + JSON.stringify(JSON.parse(sys_results), null, 2));
		/* Decrypt the /prov/net-info response */
	   	prov_results = utils.decrypt_secure_data(sys_results);
		if (prov_results !== null) {
		    var json_sys_results = JSON.parse(prov_results);
		    console.log("Decrypted Response: " + JSON.stringify(json_sys_results, null, 2));
		} else {
		    console.log('Retrying...');
		    step--;
		    substep++;
		    net_info(callback);
		}
		/* Parse the response to check if connection is done ? */
		console.log(json_sys_results.status);
		if (json_sys_results.status == 2) {
	    	    /* Send another request of CLIENT ACK */
		    console.log("Network Connection Successful...");
		    console.log("--------------------------STEP " + step + "-----------------------------");
		    step++;
		    console.log("Sending Client Acknowledgement...");
		    /* Encrypt the client acknowledgement data */
	    	    var enc_json = utils.encrypt_secure_data(json_client_ack);
	    	    options_client_ack.headers['Content-Length'] = JSON.stringify(enc_json).length;
		    options_client_ack.method = 'POST';
		    options_client_ack.path = '/prov/net-info?session_id=' + session_id;
		    console.log("Unencrypted Request Data" + JSON.stringify(JSON.parse(json_client_ack), null, 2));
		    console.log("Request: HTTP POST " + options_client_ack.path);
		    console.log(JSON.stringify(enc_json, null, 2));
		    var req2 = http.request(options_client_ack, function(res) {
			res.setEncoding('utf8');
			res.on('data', function (chunk) {
      console.log("Response: " + JSON.stringify(JSON.parse(chunk), null, 2));
      var nw_status_results = "";
      /* Decrypt the /prov/network response */
      nw_status_results = utils.decrypt_secure_data(chunk);
      if (nw_status_results !== null) {
          var json_nw_results = JSON.parse(nw_status_results);
          console.log("Decrypted Response: " + JSON.stringify(json_nw_results, null, 2));
      } else {
          console.log("Could not decrypt the /prov/network response data");
          console.log('Bad request');
          callback(1, {eflag:0, msg:"Failure : /prov/network handler failed"});
      }
			if (json_nw_results.success === 0) {
				console.log("Success : Client Acknowledgement");
				callback(1, {eflag:0, msg:"######## Provisioning Protocol Completed ########"});
			} else {
				console.log('Bad request');
				callback(1, {eflag:0, msg:"######## Provisioning Protocol Failed ########"});
			 }
			});
		    });
		    req2.on('error', function(e) {
	    		console.log('\nProblem POST with request: ' + e.message);
			console.log("Failure : Client Acknowledgement");
	    		callback(1, {eflag:0, msg:"Error HTTP POST /prov/net-info"});
		    });

		    req2.write(JSON.stringify(enc_json));
		    req2.end();
		} else {
		    attempts++;
		    /* The below code is to periodically check the network connection status.
		       Maximum number of retry attemps are 5 */
		    if (attempts >= 10) {
			console.log("Network Connection Un-Successful...");
			callback(1, {eflag:0, msg:"######## Provisioning Protocol Failed ########"});
		    } else {
			console.log('Timeout!');
			console.log('Retrying...');
			step--;
			substep++;
			net_info(callback);
		    }
		}
	    });
	});

	req.on('error', function(e) {
	    console.log('\nProblem POST with request: ' + e.message);
	    callback(1, {eflag:0, msg:"Error in sending http POST /prov/net-info"});
	});

	req.end();
	/* You can modify the below "Connection Timeout" in case the connection is taking time.
	   Otherwise, this application will not be able to send an acknowledgement.
	   Currently, it is 5 seconds with 5 retry attempts */
    }, 5000);
}


/*---------------------------------/reset-to-prov POST handler----------------------------------*/
/* This API is currently un-supported in the device so undocumented */
var options_reset_prov = {
    hostname: '192.168.250.102',
    port: 80,
    path: '/sys',
    method: 'POST',
    headers: {
	'Content-Type':'application/x-www-form-urlencoded',
	'Accept': '*/*',
    }
};

var json_reset_prov = '{"connection":{"station":{"configured":0}}}'

exports.reset_prov = function reset_prov(callback) {
    options_reset_prov.headers['Content-Length'] = json_reset_prov.length;
    options_reset_prov.method = 'POST';
    var req = http.request(options_reset_prov, function(res) {
	res.setEncoding('utf8');
	res.on('data', function (chunk) {
	    console.log("/sys/ RESET PROV RESPONSE : " + chunk);
	    callback(null, {eflag:0, msg:"Http POST ok"});
	});
    });
    req.on('error', function(e) {
	emit.log('\nProblem POST with request: ' + e.message);
	callback(1, {eflag:1, msg:"Error in sending http POST"});
    });
    req.write(json_reset_prov);
    req.end();
};
