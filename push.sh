#!/usr/bin/env bash
set -e

REPO_URL="https://github.com/Mohamedboukerche22/redbunny.git"

if [ -f "../token.txt" ]; then
  cat ../token.txt | gh auth login --with-token
elif [ -f "../tocken.txt" ]; then
  cat ../tocken.txt | gh auth login --with-token
fi

if [ -z "$(git config --global user.email)" ]; then
  git config --global user.email "mohamed@example.com"
fi

if [ -z "$(git config --global user.name)" ]; then
  git config --global user.name "Mohamed Boukerche"
fi

if git remote | grep -q "^origin$"; then
  git remote set-url origin "$REPO_URL"
else
  git remote add origin "$REPO_URL"
fi

COMMIT_MSG="$1"
if [ -z "$COMMIT_MSG" ]; then
  read -p "Enter commit message: " COMMIT_MSG
fi

if [ -z "$COMMIT_MSG" ]; then
  COMMIT_MSG="Update repository"
fi

git add -A
git commit -m "$COMMIT_MSG" || true
git push -u origin main
