hw3.out:
Authors:
Yang Li - liy38
Sam Temlock - temloo

Features:
Implements the eight command of a typical Internet Relay Chat:

USER <name>
LIST [#channel]
JOIN <#channel>
PART [#channel]
OPERATOR <password>
KICK <#channel> <user>
PRIVMSG ( <#channel> | <user> ) <message>
QUIT


Notes:
If the opt-password flag is used and the passed in value is not 20 characters or alphanumeric, then the default password is "password", this is so that when comparisons are made the string is checked against an actual value and not uninitialized garbage.

Bugs:
None that we know of