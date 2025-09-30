# xdialtester
Native application to test where dial is working fine in RDK environment.
# Build
This needs boost, websocketpp and jsoncpp. If you are building using yocto, use this command to  checkout


```devtool add --autorev xdialtester https://github.com/joseinweb/xdialtester.git  --srcbranch develop```

Add the following line in the recipe

```DEPENDS += "jsoncpp websocketpp systemd boost"```
