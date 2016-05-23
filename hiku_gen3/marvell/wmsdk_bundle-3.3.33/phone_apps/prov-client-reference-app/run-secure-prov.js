var async = require('async');
var utils = require('./utils.js');
var prov = require('./provisioning.js');
var fs = require('fs');

function startProvisioning() {
    async.series([
    	/* Establish a secure session */
	prov.secure_session,
    	/* Post network credentials */
	prov.network,
	/* Check the status of the connection */
	prov.net_info,
    ], function(err, results) {
	j = 0;
	var er = 0;
	console.log("\n---------------------LOGS------------------------");
	while(j < results.length) {
	    if (!results[j].eflag) {
		console.log(results[j].msg);
	    }
	    j++;
	}
	console.log('----------------------END------------------------');
    });
}

function die(err) {
    emit.error(err);
}

if (process.argv[2] !== undefined && process.argv[3] !== undefined && (((process.argv[4] !== "0") && process.argv[5] !== undefined) || ((process.argv[4] === "0") && process.argv[5] === undefined))) {
    pin = process.argv[2];
    network_data['ssid'] = process.argv[3];
    network_data['security'] = parseInt(process.argv[4]);
    if (process.argv[4] !== "0") {
	network_data['key'] = process.argv[5];
    }
    startProvisioning();
} else {
    console.log('Please specify command-line arguments correctly');
    console.log('Run as: \r\nnode run-secure-prov.js <prov-pin> <home-network-ssid> <home-network-security> [home-network-key]');
}
