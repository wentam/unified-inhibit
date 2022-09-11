Unit tests needed per inhibitor:
* When in monitoring mode, inhibits that get forwarded to the inhibitor result in the inhibit getting placed
* When in monitoring mode, uninhibits that get forwarded to the inhibitor result in the inhibit getting released
* Introspect returns some XML
* If the inhibiting application crashes in monitoring mode, the inhibit is released
* If the inhibiting application crashes in implementation mode, the inhibit is released
* In monitoring mode, we get inhibit/uninhibit events.
* In implementation mode, we get inhibit/uninhibit events.
