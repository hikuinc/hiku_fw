// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.

// Global containing the audio data for the current session.  Resized as 
// new buffers come in. 
gAudioBuffer <- blob(0);


//****************************************************************************
// Device callback: handle an audio buffer from the device
device.on(("sendAudioBuffer"), function(data) {
    agentLog(format("in sendAudioBuffer"));
    agentLog(format("data type=%s", typeof(data)));
    dumpBlob(data);

    // Add the new data to the audio buffer
    //gAudioBuffer.writeblob(data);
});


//****************************************************************************
// HTTP callback onrequest: called in response to HTTP request to us
http.onrequest(function(request, res){
    agentLog("in http.onrequest");

    // Handle commands and send response (TODO: remove test command)
    if (request.body == "beep") 
    {
        device.send("do_beep", 1);
        res.send(200, "OK\n");
    }
    else
    {
        res.send(400, "Command not recognized\n");
    }

    // Print the request fields to the log
    // logServerRequest(request);
});


//****************************************************************************
// Utility Functions
//****************************************************************************


//****************************************************************************
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


//****************************************************************************
// Proxy for server.log that prints a line prefix showing it is from the agent
function agentLog(str)
{
    server.log(format("AGENT: %s", str));
}


//****************************************************************************
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
