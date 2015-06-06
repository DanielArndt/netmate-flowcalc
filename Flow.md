# Introduction #

Described herein are some definitions of Flows and some of their associated properties .

## Definition of the forward/backward direction ##

The forward direction is the direction in which the first packet in a flow traveled (often referred to as the client) which is defined in terms of the [Features](Features.md) a packets from srcip on srcport to dstip on dstport. The backward direction is the reverse of the forward direction.

## Definition of a sub-flow ##

A sub-flow is (as the name implies) a flow within a flow. Sub flows are separated by a period of inactivity exceeding that of `Idle_Threshold`, as set in the RulesFile (default=1000000 or 1 second).