README about feature packs
--------------------------

CyaSSL Feature packs is a concept added specially for WMSDK
releases. As per licensing terms, CyaSSL releases are given out in
binary format i.e. only library and header files are released as part
of WMSDK. Different set of CyaSSL features would be needed for each
customer product. Removal of unnecessary features would help cut down
footprint requirements.

Since the user cannot recompile CyaSSL, Marvell provides a collection
of pre-compiled tarballs each with different set of features. A set of
features in a tarball is called a feature pack. Two tarballs are
supplied for each release. One is debug and another is production
version.

-------
Note: Just taking a debug library will not actually enable debugging
logs. Please also enable TLS debug though 'make menuconfig' -->
  --> Development and Debugging --> Enable Debugging Output
  --> Transport Layer Security

AND

enable Cyassl debugging
'make menuconfig' --> Modules --> TLS --> Enable CyaSSL debug

You can also set debug level at the same path as above in 'make menuconfig'
-------

Please have a look at the feature pack excel sheet present in the
cyassl feature packs directory for more information about the features
in each feature pack.

The SDK selects default feature pack i.e. fp0. The header files and
libraries for this feature pack are present at
wmsdk-bundle-x.x.x/cyassl-feature-pack/<arch>/cyassl-<version>-<fpX>
directory.  If you need to use a different feature pack you can pass
the feature pack name to make.

For e.g. if you need feature pack 3
make CYASSL_FEATURE_PACK=fp3
If you need debug version of feature pack 3
make CYASSL_FEATURE_PACK=fp3_debug

If you need a feature pack which is not supplied with the WMSDK
release, please contact Marvell support.

