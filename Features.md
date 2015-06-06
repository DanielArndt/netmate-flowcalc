

# Introduction #

A quick description of the features which are ouput by netmate-flowcalc.

# Features #

Below are the features output by netmate-flowcalc. Each [Flow](Flow.md) is represented by this set of features, the first 5 of which are the 5-tuple which by definition define a [Flow](Flow.md) (srcip, srcport, dstip, dstport, proto) for a specific point in time.

All features are represented as integers unless otherwise stated.

  * **srcip** - (_string_) The source ip address
  * **srcport** - The source port number
  * **dstip** - (_string_) The destination ip address
  * **dstport** - The destination port number
  * **proto** - The protocol (ie. TCP = 6, UDP = 17)
  * **total\_fpackets** - Total packets in the forward direction
  * **total\_fvolume** - Total bytes in the forward direction
  * **total\_bpackets** - Total packets in the backward direction
  * **total\_bvolume** - Total bytes in the backward direction
  * **min\_fpktl** - The size of the smallest packet sent in the forward direction (in bytes)
  * **mean\_fpktl** - The mean size of packets sent in the forward direction (in bytes)
  * **max\_fpktl** - The size of the largest packet sent in the forward direction (in bytes)
  * **std\_fpktl** - The standard deviation from the mean of the packets sent in the forward direction (in bytes)
  * **min\_bpktl** - The size of the smallest packet sent in the backward direction (in bytes)
  * **mean\_bpktl** - The mean size of packets sent in the backward direction (in bytes)
  * **max\_bpktl** - The size of the largest packet sent in the backward direction (in bytes)
  * **std\_bpktl** - The standard deviation from the mean of the packets sent in the backward direction (in bytes)
  * **min\_fiat** - The minimum amount of time between two packets sent in the forward direction (in microseconds)
  * **mean\_fiat** - The mean amount of time between two packets sent in the forward direction (in microseconds)
  * **max\_fiat** - The maximum amount of time between two packets sent in the forward direction (in microseconds)
  * **std\_fiat** - The standard deviation from the mean amount of time between two packets sent in the forward direction (in microseconds)
  * **min\_biat** - The minimum amount of time between two packets sent in the backward direction (in microseconds)
  * **mean\_biat** - The mean amount of time between two packets sent in the backward direction (in microseconds)
  * **max\_biat** - The maximum amount of time between two packets sent in the backward direction (in microseconds)
  * **std\_biat** - The standard deviation from the mean amount of time between two packets sent in the backward direction (in microseconds)
  * **duration** - The duration of the flow (in microseconds)
  * **min\_active** - The minimum amount of time that the flow was active before going idle (in microseconds)
  * **mean\_active** - The mean amount of time that the flow was active before going idle (in microseconds)
  * **max\_active** - The maximum amount of time that the flow was active before going idle (in microseconds)
  * **std\_active** - The standard deviation from the mean amount of time that the flow was active before going idle (in microseconds)
  * **min\_idle** - The minimum time a flow was idle before becoming active (in microseconds)
  * **mean\_idle** - The mean time a flow was idle before becoming active (in microseconds)
  * **max\_idle** - The maximum time a flow was idle before becoming active (in microseconds)
  * **std\_idle** - The standard devation from the mean time a flow was idle before becoming active (in microseconds)
  * **sflow\_fpackets** - The average number of packets in a sub flow in the forward direction
  * **sflow\_fbytes** - The average number of bytes in a sub flow in the forward direction
  * **sflow\_bpackets** - The average number of packets in a sub flow in the backward direction
  * **sflow\_bbytes** - The average number of packets in a sub flow in the backward direction
  * **fpsh\_cnt** - The number of times the PSH flag was set in packets travelling in the forward direction (0 for UDP)
  * **bpsh\_cnt** - The number of times the PSH flag was set in packets travelling in the backward direction (0 for UDP)
  * **furg\_cnt** - The number of times the URG flag was set in packets travelling in the forward direction (0 for UDP)
  * **burg\_cnt** - The number of times the URG flag was set in packets travelling in the backward direction (0 for UDP)
  * **total\_fhlen** - The total bytes used for headers in the forward direction.
  * **total\_bhlen** - The total bytes used for headers in the backward direction.