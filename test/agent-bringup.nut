//**********************************************************************
// Ping-pong
device.on(("ping"), function(msg) {
    server.log("AGENT: in ping");
    device.send("pong", "from agent");
});

