#!/bin/sh -x

linux_path=~/source/linux/linux
reference="fate#320291"

for p in `cat patchlist`;do
	./scripts/patch-tags-from-git $p $linux_path
	./scripts/patch-tag --Add Signed-off-by="Coly Li <colyli@suse.de>" $p 
	./scripts/patch-tag --add Reference=$reference $p
	./scripts/patch-tag --delete Mime-version --delete Content-type --delete Content-transfer-encoding $p
done
