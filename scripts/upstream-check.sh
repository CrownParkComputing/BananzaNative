#!/bin/bash
# Upstream (NotRexed/Hookshot) against this tree: what is new since .upstream-sha, whether a merge is clean, and --merge / --mark.
set -e
cd "$(dirname "$0")/.."
git remote get-url upstream >/dev/null 2>&1 || git remote add upstream https://github.com/NotRexed/Hookshot.git
git fetch -q upstream main
LAST=$(cat .upstream-sha)
NEW=$(git rev-parse upstream/main)
case "${1:-}" in
  --mark) M=$(git merge-base HEAD upstream/main); echo "$M" > .upstream-sha; git add .upstream-sha; git commit -q -m "Merged upstream Hookshot $M"; echo "marked $M (what this branch contains; upstream head $NEW)"; exit 0;;
  --merge)
    git merge --no-edit upstream/main && echo "merged upstream/main ($NEW): rebuild (lift + build), test, then scripts/upstream-check.sh --mark"
    exit $?;;
esac
if [ "$LAST" = "$NEW" ]; then echo "upstream unchanged ($NEW)"; exit 0; fi
echo "upstream has $(git rev-list --count "$LAST".."$NEW") new commits since $LAST:"
git log --oneline "$LAST".."$NEW" | head -40
echo
if git merge-tree "$(git merge-base HEAD upstream/main)" HEAD upstream/main | grep -q '^<<<<<<<'; then
  echo "a merge would CONFLICT in:"; git merge-tree "$(git merge-base HEAD upstream/main)" HEAD upstream/main | grep -B1 '^<<<<<<<' | grep -v '^<<<<<<<\|^--' | sort -u | head
else
  echo "a merge would be clean: scripts/upstream-check.sh --merge"
fi
