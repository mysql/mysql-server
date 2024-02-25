

## I. Verify that call to sleep SQL is terminated after disconnection
#
# 1. Send 1 hour sleep command to server through X Protocol
# 2. Disconnect without waiting for response
# 3. Verify that sleep was terminated (almost right away)


#
# I.1
RUN SELECT SLEEP(60 * 60 /* 60 minutes */);

#
# I.2
ok

#
# I.3 Wait maximum of 30 seconds for closure of X Connection
Success, there is only one session left
