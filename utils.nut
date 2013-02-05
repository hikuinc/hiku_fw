// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.

//****************************************************************************
// Utility functions that can be copied and pasted into EImp code
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
// Prints out the key value pairs of a Squirrel table
function printTableFields(data)
{
    server.log(data);
    foreach (i, j in data)
    {
        server.log(format("%s=%s", i.tostring(), j.tostring()));
    }
}


//****************************************************************************
// Proxy for server.log that prints a line prefix showing it is from the agent
function agentLog(str)
{
    server.log(format("AGENT: %s", str));
}
