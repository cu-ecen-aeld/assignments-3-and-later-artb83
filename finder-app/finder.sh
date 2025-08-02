#!/bin/sh
#$1 path to directory where the script shall search for a string
#$2 the string to find within the given directory ($1)

N_MATCHING_FILES=0
N_MATCHING_LINES=0
#Check valid amount of parameters
if [ $# != 2 ]; then
    echo "Invalid parameters."
    exit 1
fi
#If provided path is valid, search for the pattern, otherwise exit with error
if [ -d $1 ]; then
    N_MATCHING_FILES="$(grep -Rl $2 $1 | wc -l)"
    N_MATCHING_LINES="$(grep -rc $2 $1 | wc -w)"
else
    echo $1" is not a directory or a filename."
    exit 1
fi


echo "The number of files are "$N_MATCHING_FILES" and the number of matching lines are "$N_MATCHING_LINES

exit 0
