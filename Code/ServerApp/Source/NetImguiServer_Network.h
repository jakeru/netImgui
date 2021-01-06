#pragma once


namespace NetImguiServer { namespace Network
{
// Initialize application's networking
bool Startup();

// Shutdown application's networking
void Shutdown();

// True if this application is actively waiting for clients to reach it
// Note: False when unable to open listen socket (probably because port number alreay in use)
bool IsWaitingForConnection();

}}
