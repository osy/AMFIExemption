# AMFIExemption

Adds allowed entitlements to non-Apple signed apps when SIP is enabled.

## What is this?

When SIP and AMFI are enabled, applications cannot use some entitlements even if they are root. This is by design and is good for security. If we want to use a special entitlement, there has been traditionally two approaches.

1. [Patch AMFI to disable entitlement checks][1]
2. Add the boot-arg `amfi_get_out_of_my_way=1` and disable SIP

Neither are great for non-development use because they effectively disable a large security feature. Instead, we wish to "selectively" weaken the security by permitting certain Apple private entitlements to be granted to any app. This is because some entitlement allow great harm (such as accessing keychain data) while other entitlements allow access to Apple private features (such as disabling the transparency of the menu bar). If we give every app the ability to use any entitlement then to use private features we also have live with any app being able to access the keychain (for example).

Please note that care should be taken with this KEXT because any entitlement you add to the exemptions list can be granted to **any** application. If you add sensitive entitlements, you have no more security than just disabling SIP+AMFI. You should think about granting an entitlement the same way you would think about making a kernel patch: it is your responsibility to make sure you do not introduce security holes.

## Usage

This KEXT requires [Lilu][2].

To add an entitlement exemption, open `Info.plist` in your favourite plist editor and under `IOKitPersonalities => AMFIExemptionList => Exemptions`, you can add new strings to the array. There are two kinds of exemptions supported:

* Prefix matching: the last character of the identifier is `*` and every entitlement whose prefix matches the string before the `*` will be removed from entitlement checks. For example `com.apple.private.*` will allow any app to have any entitlement that begins with `com.apple.private.`. Note this is just an example and is highly discouraged!
* Absolute matching: the full identifier of the entitlement such as `com.apple.private.CoreGraphics.debugging`.

Note that having an entitlement in the exemption list does *not* mean that entitlement is granted to all apps. Your app must still be code-signed with the requested entitlement. This KEXT only disables the requirement that most entitlements require an Apple CA anchor. You can either self-sign with your own certificate or ad-hoc sign with no certificate and still use the entitlement.

## Security

It is highly recommended that this KEXT is loaded by OpenCore with the vault feature enabled. This would prevent malware from modifying `Info.plist` and granting itself arbitrary entitlements which defeats SIP+AMFI security.

  [1]: https://pvieito.com/2016/12/amfid-patching
  [2]: https://github.com/acidanthera/Lilu