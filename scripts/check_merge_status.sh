#!/bin/bash 

submit_branches=`git branch -la| grep colyli`

for branch in $submit_branches;do
	product=`dirname $branch| xargs basename`
	product_branch="remotes/origin/$product"
	echo "submit branch $branch is for product branch $product_branch"
	git merge-base --is-ancestor $branch $product_branch
	ret=$?
	if [ $ret == 0 ] ;then
		echo "merged: $branch is merged into $product_branch"
	else
		echo "NOT merged: $branch is NOT merged into $product_branch"
	fi
done
