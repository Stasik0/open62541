#!/usr/bin/env python

# coding: UTF-8
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
# It is based on the idea of http://0pointer.net/blog/projects/copyright.html

import os
import re
import io
import sys

from git import *
from shutil import move

try:
    from StringIO import StringIO
except ImportError:
    from io import StringIO

if sys.version_info[0] >= 3:
    # strings are already parsed to unicode
    def unicode(s):
        return s


# only use lower case name
authorFullName = {
    'staldert': 'Thomas Stalder'
}

skipNames = ['=', 'open62541']

def compactYears(yearList):

    current = None
    last = None
    result = []

    for yStr in yearList:
        y = int(yStr)
        if last is None:
            current = y
            last = y
            continue

        if y == last + 1:
            last = y
            continue

        if last == current:
            result.append("%i" % last)
        else:
            result.append("%i-%i" % (current, last))

        current = y
        last = y

    if not last is None:
        if last == current:
            result.append("%i" % last)
        else:
            result.append("%i-%i" % (current, last))

    return ", ".join(result)

fileAuthorStats = dict()

def insertCopyrightAuthors(file, authorsList):
    copyrightEntries = list()
    for author in authorsList:
        copyrightEntries.append(unicode("Copyright {} (C) {}").format(compactYears(author['years']), author['author']))

    copyrightAdded = False
    commentPattern = re.compile(r"(.*)\*/$")

    tmpName = file + ".new"
    tempFile = io.open(tmpName, mode="w", encoding="utf-8")
    with io.open(file, mode="r", encoding="utf-8") as f:
        for line in f:
            if copyrightAdded or not commentPattern.match(line):
                tempFile.write(line)
            else:
                tempFile.write(commentPattern.match(line).group(1) + "\n *\n")
                for e in copyrightEntries:
                    tempFile.write(unicode(" *    {}\n").format(e))
                tempFile.write(unicode(" */\n"))
                copyrightAdded = True
    tempFile.close()
    os.unlink(file)
    move(tmpName, file)

def updateCopyright(repo, file):
    print("Checking file {}".format(file))

    # Build the info on how many lines every author commited every year
    relativeFilePath = file[len(repo.working_dir)+1:].replace("\\","/")

    if not relativeFilePath in fileAuthorStats:
        print("File not found in list: {}".format(relativeFilePath))
        return

    stats = fileAuthorStats[relativeFilePath]

    # Now create a sorted list and filter out small contributions
    authorList = list()

    for authorStr in stats:
        if authorStr in skipNames:
            continue

        author = unicode(authorStr)

        authorYears = list()
        for year in stats[author]:
            if stats[author][year] < 10:
                # ignore contributions for this year if less than 10 lines changed
                continue
            authorYears.append(year)
        if len(authorYears) == 0:
            continue
        authorYears.sort()

        if author.lower() in authorFullName:
            authorName = authorFullName[author.lower()]
        else:
            authorName = author


        authorList.append({
            'author': authorName,
            'years': authorYears
        })

    # Sort the authors list first by year, and then by name

    authorListSorted = sorted(authorList, key=lambda a: (a['years'], a['author']))
    insertCopyrightAuthors(file, authorListSorted)


# This is required since some commits use different author names for the same person
assumeSameAuthor = {
    'Mark': 'Mark Giraud',
    'Infinity95': 'Mark Giraud',
    'janitza-thbe': 'Thomas Bender',
    'Stasik0': 'Sten Grüner',
    'Sten': 'Sten Grüner',
    'Frank Meerkoetter': 'Frank Meerkötter',
    'ichrispa': 'Chris Iatrou',
    'Chris Paul Iatrou': 'Chris Iatrou',
    'Torben-D': 'TorbenD',
    'FlorianPalm': 'Florian Palm',
}

def buildFileStats(repo):

    fileRenameMap = dict()
    renamePattern = re.compile(r"(.*){(.*) => (.*)}(.*)")

    cnt = 0
    for commit in repo.iter_commits():
        cnt += 1

    curr = 0
    for commit in repo.iter_commits():
        curr += 1
        print("Checking commit {}/{}  ->   {}".format(curr, cnt, commit.hexsha))

        for objpath, stats in commit.stats.files.items():

            match = renamePattern.match(objpath)

            if match:
                # the file was renamed, store the rename to follow up later
                oldFile = (match.group(1) + match.group(2) + match.group(4)).replace("//", "/")
                newFile = (match.group(1) + match.group(3) + match.group(4)).replace("//", "/")

                while newFile in fileRenameMap:
                    newFile = fileRenameMap[newFile]

                if oldFile != newFile:
                    fileRenameMap[oldFile] = newFile
            else:
                newFile = fileRenameMap[objpath] if objpath in fileRenameMap else objpath

            if stats['insertions'] > 0:
                if not newFile in fileAuthorStats:
                    fileAuthorStats[newFile] = dict()

                authorName = commit.author.name
                if authorName in assumeSameAuthor:
                    authorName = assumeSameAuthor[authorName]

                if not authorName in fileAuthorStats[newFile]:
                    fileAuthorStats[newFile][authorName] = dict()

                if not commit.committed_datetime.year in fileAuthorStats[newFile][authorName]:
                    fileAuthorStats[newFile][authorName][commit.committed_datetime.year] = 0

                fileAuthorStats[newFile][authorName][commit.committed_datetime.year] += stats['insertions']




def walkFiles(repo, folder, pattern):
    patternCompiled = re.compile(pattern)
    for root, subdirs, files in os.walk(folder):
        for f in files:
            if patternCompiled.match(f):
                fname = os.path.join(root,f)
                updateCopyright(repo, fname)

if __name__ == '__main__':
    baseDir = os.path.abspath(os.path.join(os.path.dirname(os.path.realpath(__file__)), os.pardir))
    repo = Repo(baseDir)
    assert not repo.bare

    buildFileStats(repo)

    dirs = ['src', 'plugins', 'include']

    for dir in dirs:
        walkFiles(repo, os.path.join(baseDir, dir), r"(.*\.c|.*\.h)$")