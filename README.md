# QuarkIM

A tiny and lovely instant messaging system, with some kind of security support.

# Why QuarkIM?

One day, one of my friends asked me about which instant messanger is the most
safe one (because he is doing something really *private* and do not want his
messages read by others except his peer). At that time, I cannot answer. No
matter how the data is encrypted, the data could still got leaked if *someone*
really want the data (I am not meaning it technically... for example... by
policy maybe?). This is the first reason that I want to write this. So:

* The IM server/client is open sourced, so it's... open... and secure...
* You can just compile and run your own server. It's MORE secure!
* All the messages will be encrypted.

OK... I confess... I just write this for fun... :)

# System Model

This is a very generic client/server model. Multiple clients could connect to
the same server, then they could talk to each other. Server is the middle
man. For now I am not planning to give permission to talk directly between
clients.

# How Messages Are Encrypted?

I am security newbie, however I think this should work:

First of all, no matter we are client or server node, we have one RSA pair
(public key P and private key P). If there is two nodes A and B with their two
pair of keys, and if A wants to send something (msg) to B, then what we want
should be something like: Pb(Xa(msg)), which means first encrypt the message
using A's private key, then again using B's public key. Everyone needs to
exchange their public key before all their talks.

# Libraries Required

None.
