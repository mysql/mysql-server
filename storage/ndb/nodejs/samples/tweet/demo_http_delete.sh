#
#
# Before running these, start the tweet.js app as a web server:
#
#       node tweet start server 7800
#
#


# Delete everyone.  Afterwards the database will be empty
# due to cascading deletes.
echo
echo "Deleting users:"
curl -X DELETE http://localhost:7800/user/caligula
curl -X DELETE http://localhost:7800/user/uncle_claudius
curl -X DELETE http://localhost:7800/user/agrippina
curl -X DELETE http://localhost:7800/user/nero


# See that the tweets are all gone:
echo
echo "Fetching the most recent tweets:"
curl http://localhost:7800/tweets-recent/10  # empty


# Try to delete someone a second time & you get an error:
echo
echo "Deleting a user again:"
curl -X DELETE http://localhost:7800/user/uncle_claudius
