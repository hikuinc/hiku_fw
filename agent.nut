// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.


// Global containing the audio data for the current session.  Resized as 
// new buffers come in.  
gAudioBuffer <- blob(0);
gChunkCount <- 0;


//**********************************************************************
// Send the barcode to hiku's server
function sendToBeepIt(data)
{
    // Error checking (TODO: improve)
    if (!"scandata" in data)
    {
        agentLog("Error: no barcode data to send");
        return;
    }

    // Build the packet to the current specs
    local rawData = "BeepIt=00.01\n";
    foreach (k, v in data)
        rawData += k.tostring() + "=" + v.tostring() + "\n";
    rawData = http.urlencode({rawData=rawData});

    // Send the packet to our server
    // TODO: verify the headers, consider using sendasync()
    local res = http.post(
            "http://www.beepit.us/prod/cgi-bin/readRawDeviceData.py",
            {"Content-type": "application/x-www-form-urlencoded", 
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
}


//**********************************************************************
// Receive and send out the beep packet
device.on(("uploadBeep"), function(data) {
    //agentLog("in uploadBeep");

    if (!data.scandata)
    {
        agentLog("Failed to receive barcode");
        device.send("uploadCompleted", "failure");
    }
    else
    {
        agentLog(format("Barcode received: %s", data.scandata));
        local enableHikuServer = true;

        // TODO: disabled for bring-up
        //enableHikuServer = false;
        if (enableHikuServer)
        {
            sendToBeepIt(data);  
        }
        else
        {
            agentLog("(sending to hiku server not enabled)");
        }
        device.send("uploadCompleted", "success");
    }
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
device.on(("endAudioUpload"), function(numChunksTotal) {
    //agentLog("in endAudioUpload");

    // For now, we assume we have all the data, and responsibility 
    // for ensuring this is on the device. 

    // If  no audio data, just exit
    if (gAudioBuffer.len() == 0)
    {
        agentLog("No audio data to send to server.");
        return;
    }

    if (gChunkCount != numChunksTotal)
    {
        server.log(format("ERROR: expected %d chunks, got %d"), 
                   numChunksTotal, gChunkCount);
    }
    
    // Send audio to server
    agentLog(format("Audio ready to send. Size=%d", gAudioBuffer.len()));
    local req = http.post("http://bobert.net:4444", 
                         {"Content-Type": "application/octet-stream"}, 
                         http.base64encode(gAudioBuffer));
    // TODO: if the server is down, this will block all other events
    // until it times out.  Events seem to be queued on the server 
    // with no ill effects.  They do not block the device.  Move 
    // to async? 
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


//**********************************************************************
// Utility Functions
//**********************************************************************


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
function dumpTable(data)
{
    foreach (k, v in data)
    {
        if (typeof v == "table")
        {
            agentLog(">>>");
            dumpTable(v);
            agentLog("<<<");
        }
        else
        {
            agentLog(k.tostring() + "=" + v.tostring());
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

