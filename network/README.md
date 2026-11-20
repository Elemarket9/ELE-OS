# Network access

Per-app network on/off control for the Cuttlefish product
(`ele_os_cf_x86_64`) and the Treble GSI (`ele_os_gsi_arm64`).

The choice is granted by default. Turning it off means that app should see
the network as down (`ENETUNREACH`). One switch covers that app's IP
traffic. The cellular baseband is outside this control.

`EleNetwork` is a launcher screen. It stores the choice in the app-private
file `network-policy.txt`. The format is UTF-8: comments start with `#`,
and a denial is one line, `package.name off`. A package with no line is
granted.

Host tests cover that decision and the file format. They do not open a
socket. This tree does not install a netd rule, a packet filter, or a VPN.
No Cuttlefish boot and no GSI boot are recorded here, so traffic blocking
is not shown.

`sh tests/network.sh` runs the host checks. `sh tests/run.sh` includes it.
