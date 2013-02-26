// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.


// Global containing the audio data for the current session.  Resized as 
// new buffers come in.  
gAudioBuffer <- blob(0);


//**********************************************************************
// Receive and send out the beep packet
device.on(("uploadBeep"), function(data) {
    //agentLog("in uploadBeep");

    if (!data.barcode)
    {
        agentLog("Failed to receive barcode");
        device.send("uploadCompleted", "failure");
    }
    else
    {
        agentLog(format("Barcode received: %s", data.barcode));
        device.send("uploadCompleted", "success");
    }
});


//**********************************************************************
// Prepare to receive audio from the device
device.on(("startAudioUpload"), function(data) {
    //agentLog("in startAudioUpload");

    // Reset our audio buffer
    gAudioBuffer.resize(0);
});


//**********************************************************************
// Send complete audio sample to the server
device.on(("endAudioUpload"), function(data) {
    //agentLog("in endAudioUpload");

    // TODO: Will we have all of our audio data here?  It is coming in 
    // asynchronously, so we may have more, right?  How to handle? 
    // For now, we assume we have all the data, since A-law data is 
    // dropped if it does not have a full buffer. However, that is a 
    // bad assumption, since these events are all asynchronous. 

    // If  no audio data, just exit
    if (gAudioBuffer.len() == 0)
    {
        agentLog("No audio data to send to server.");
        return;
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

