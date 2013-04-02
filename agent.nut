// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.


server.log(format("Agent started, external URL=%s", http.agenturl()));


gAudioBuffer <- blob(0); // Contains the audio data for the current 
                         // session.  Resized as new buffers come in.  
gChunkCount <- 0; // Used to verify that we got the # we expected
gLinkedRecord <- ""; // Used to link unknown UPCs to audio records. 
                     // We will set this when we get a request to do
                     // so from the server, then clear it after we 
                     // send the next audio beep, after next barcode, 
                     // or after a timeout
gLinkedRecordTimeout <- null; // Will clear gLinkedRecord after this time
gImpeeIdResponses <- []; // Table of responses to the getImpeeId request


//======================================================================
// Beep handling 

//**********************************************************************
// Send the barcode to hiku's server
function sendBeepToHikuServer(data)
{
    local disableSendToServer = false;
    //disableSendToServer = true;
    if (disableSendToServer)
    {
        agentLog("(sending to hiku server not enabled)");
        return;
    }
    
    /*
    local useOldFormat = false;
    //useOldFormat = true;
    // If using the original Hiku server, enable this
    if (useOldFormat)
    {
        // Build the packet to the original Hiku server specs
        local rawData = "BeepIt=00.01\n";
        foreach (k, v in data)
            rawData += k.tostring() + "=" + v.tostring() + "\n";
        rawData = http.urlencode({rawData=rawData});

        // Send the packet to our server
        local res = http.post(
                "http://www.beepit.us/prod/cgi-bin/readRawDeviceData.py",
                {"Content-Type": "application/x-www-form-urlencoded", 
                "Accept": "text/plain"}, 
                rawData).sendsync();

        // Check response status code
        if (res.statuscode != 200)
        {
            agentLog(format("Error: got status code %d, expected 200", 
                        res.statuscode));
        }
        else
        {
            agentLog("Barcode accepted by hiku server");
        }

        device.send("uploadCompleted", "success");
        return;
    }
    */

    // Special handling for audio beeps 
    if (data.scandata == "")
    {
        // Encode the audio data as base64, and store the size. The 
        // "scansize" parameter is applicable for both audio and barcodes. 
        data.audiodata = http.base64encode(gAudioBuffer);
        data.scansize = data.audiodata.len();

        // If not expired, attach the current linkedrecord (usually 
        // blank). Then reset the global. 
        if (gLinkedRecordTimeout && time() < gLinkedRecordTimeout)
        {
            data.linkedrecord = gLinkedRecord;
        }
        gLinkedRecord = ""; 
        gLinkedRecordTimeout = null;
    }
        
    // URL-encode the whole thing
    data = http.urlencode(data);
    //agentLog(data);

    // Create and send the request
    agentLog("Sending beep to server...");
    local req = http.post(
            //"http://bobert.net:4444", 
            "http://srv2.hiku.us/scanner_1/imp_beep",
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            data);
    // TODO: if the server is down, this will block all other events
    // until it times out.  Events seem to be queued on the server 
    // with no ill effects.  They do not block the device.  Move 
    // to async? 
    local res;
    local transactionTime = time();
    res = req.sendsync();
    transactionTime = time() - transactionTime;
    server.log(format("Server transaction time: %ds", transactionTime));

    // Handle the response
    local returnString = "success";

    if (res.statuscode != 200)
    {
        returnString = "failure"
        agentLog(format("Error: got status code %d, expected 200", 
                    res.statuscode));
    }
    else
    {
        // TODO DEBUG remove
        agentLog(res.body);

        // Parse the response (in JSON format)
        local body = http.jsondecode(res.body);

        try 
        {
            // Handle the various non-OK responses.  Nothing to do for "ok". 
            if (body.status != "ok")
            {
                // Possible causes: speech2text failure, unknown.
                if ("error" in body.cause)
                {
                    returnString = "failure";
                    agentLog(format("Error: server responded with %s", 
                                         body.cause.error));
                }
                // Possible causes: unknown UPC code
                else if ("linkedrecord" in body.cause)
                {
                    gLinkedRecord = body.cause.linkedrecord;
                    gLinkedRecordTimeout = time()+10; // in seconds
                    returnString = "unknown-upc";
                    agentLog("Error: unknown UPC code");
                }
                // Unknown response type
                else
                {
                    returnString = "failure";
                    agentLog("Error: unexpected cause in response");
                }
            }
        }
        catch(e)
        {
            server.log(format("Caught exception: %s", e));
            returnString = "failure";
            agentLog("Error: malformed response");
        }
    }

    // Return status to device
    device.send("uploadCompleted", returnString);
}


//**********************************************************************
// Receive and send out the beep packet
device.on(("uploadBeep"), function(data) {
    gLinkedRecord = "";  // Clear on next (i.e. this) barcode scan
    gLinkedRecordTimeout = null;
    sendBeepToHikuServer(data);  
});


//**********************************************************************
// Prepare to receive audio from the device
device.on(("startAudioUpload"), function(data) {
    //agentLog("in startAudioUpload");

    // Reset our audio buffer
    gAudioBuffer.resize(0);
    gChunkCount = 0;
});


//**********************************************************************
// Send complete audio sample to the server
device.on(("endAudioUpload"), function(data) {
    //agentLog("in endAudioUpload");

    // If  no audio data, just exit
    if (gAudioBuffer.len() == 0)
    {
        agentLog("No audio data to send to server.");
        return;
    }

    if (gChunkCount != data.scansize)
    {
        agentLog(format("ERROR: expected %d chunks, got %d", 
                   data.scansize, gChunkCount));
    }

    if (data.scandata != "")
    {
        agentLog("Error: found barcode when expected only audio data");
    }

    local sendToDebugServer = false;
    //sendToDebugServer = true;
    if (sendToDebugServer)
    {
        // Send audio to server
        agentLog(format("Audio ready to send. Size=%d", gAudioBuffer.len()));
        local req = http.post("http://bobert.net:4444", 
                             {"Content-Type": "application/octet-stream"}, 
                             http.base64encode(gAudioBuffer));
        local res = req.sendsync();

        if (res.statuscode != 200)
        {
            agentLog("An error occurred:");
            agentLog(format("statuscode=%d", res.statuscode));
            device.send("uploadCompleted", "failure");
        }
        else
        {
            agentLog("Audio sent to server.");
            device.send("uploadCompleted", "success");
        }

        return;
    }

    sendBeepToHikuServer(data);  
});


//**********************************************************************
// Handle an audio buffer from the device
device.on(("uploadAudioChunk"), function(data) {
    //agentLog(format("in device.on uploadAudioChunk"));
    //agentLog(format("chunk length=%d", data.length));
    //dumpBlob(data.buffer);

    // Add the new data to the audio buffer, truncating if necessary
    data.buffer.resize(data.length);  // Most efficient way to truncate? 
    gAudioBuffer.writeblob(data.buffer);
    gChunkCount++;
});


//======================================================================
// External HTTP request handling

//**********************************************************************
// Handle incoming requests to my external agent URL.  
// Responses are queued and serviced when the device responds. 
// TODO: support multiple response types in the queue. Also right now
// it assumes the same response for all requests of the same type. 
http.onrequest(function (request, res)
{
    // Handle supported requests
    if (request.path == "/getImpeeId") {
        gImpeeIdResponses.push(res);
        device.send("request", "getImpeeId");
    } else {
      agentLog(format("AGENT Error: unexpected path %s", request.path));
      res.send(400, format("unexpected path %s", request.path));
  }
});


//**********************************************************************
// Receive impee ID from the device and send to the external requestor 
device.on(("impeeId"), function(id) {
    foreach(res in gImpeeIdResponses) {
        res.send(200, id);
    }
    gImpeeIdResponses = [];
});


//======================================================================
// Utility Functions

//**********************************************************************
// Print all ServerRequest fields 
function logServerRequest(request)
{
    agentLog(format("request.body: \"%s\"", request.body));
    agentLog(format("request.headers:"));
    foreach (key, value in request.headers)
    {
        agentLog(format("-->   \"%s=%s\"", key, value));
    }
    agentLog(format("request.method: \"%s\"", request.method));
}


//**********************************************************************
// Proxy for server.log that prints a line prefix showing it is from the agent
function agentLog(str)
{
    server.log(format("AGENT: %s", str));
}


//**********************************************************************
// Print the contents of a table
function dumpTable(data, prefix="")
{
    foreach (k, v in data)
    {
        if (typeof v == "table")
        {
            agentLog(prefix + k.tostring() + " {");
            dumpTable(v, prefix+"-");
            agentLog(prefix + "}");
        }
        else
        {
            if (v == null)
            {
                v = "(null)"
            }
            agentLog(prefix + k.tostring() + "=" + v.tostring());
        }
    }
}


//**********************************************************************
// Print the contents of a blob to the log in a formatted way
function dumpBlob(data)
{
    // Constants
    const cBlob8BitSigned = 'c';
    const cBlob16BitSigned = 's';
    const cBlob16BitUnsigned = 'w';

    // Output parameters
    local dataType = cBlob8BitSigned; // Type of data stored (see
                                      // blob documentation)
    local elementsPerLine = 12; // Number of elements to print per line
    local linesToDump = 10; // Max number of lines to dump

    local str = ""; 
    local elements = 0;
    local lines = 0;

    data.seek(0);
    while(!data.eos())
    {
        // If too many lines, indicate there is more data not printed,
        // drop any current data, and exit the loop
        if (lines > linesToDump)
        {
            agentLog("(truncated...)");
            elements = 0; 
            break;
        }

        // Get the next element
        str += data.readn(dataType) + " ";
        elements++;

        // If we have a full line, print it out 
        if (elements >= elementsPerLine)
        {
            agentLog(str);
            str = "";
            elements = 0;
            lines++;
        }
    }
    if (elements > 0)
    {
        // Got to end of buffer with less than a full line. Print remainder. 
        agentLog(str);
    }
}

