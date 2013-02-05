// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.


//****************************************************************************
// Device callback: handle an audio buffer from the device
device.on(("sendAudioBuffer"), function(data) {
    agentLog(format("in sendAudioBuffer"));
    agentLog(format("data type=%s", type(data)));
});


//****************************************************************************
// HTTP callback: onrequest
http.onrequest(function(request, res){
    agentLog("in http.onrequest");

    // Handle commands and send response
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
