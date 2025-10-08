# Gemini Live Controller Documentation

This document outlines the correct procedure for initializing the Gemini Live API with a system prompt or context for the `gemini_live_controller.py` script.

## Providing a System Prompt

To ensure the model responds in the desired persona and language, a system prompt must be sent to the API. The key takeaway from the reference implementation in the `live-api-web-console` is that this context should be sent as the first message *after* a successful connection is established, not as a parameter during the connection setup.

The `gemini_live_controller.py` script was previously attempting to pass a `context` argument during the connection call, which is not supported by the Python `google-generativeai` library and resulted in an error.

### Correct Implementation

The correct sequence of operations is:

1.  **Connect** to the Gemini Live API using `client.aio.live.connect()`.
2.  **Send the system prompt** immediately after connecting using the `session.send_client_content()` method. This message should contain the desired persona and instructions for the model.

This approach ensures that the model is properly configured before it starts processing any user input.