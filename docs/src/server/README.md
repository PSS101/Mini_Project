# Server folder

## server.c
- handle_signal(): handles shutdown signals.
- active_expiry_thread(): runs background expiry cleanup.
- handle_client(): processes one client connection.
- start_server(): starts listening for new connections.

## Workflow
The server accepts requests from clients, forwards them to the engine, and returns replies.
