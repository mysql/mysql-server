# Sample Application

### API example code

Here we provide short but complete and working samples of some of the most 
important parts of the Jones API.

- [find.js](find.js) illustrates using [session.find()]
(../../database-jones/API-documentation/Session) 
to retreive a single record from a table.
- [insert.js](insert.js) illustrates using session.persist() to store a record.
- [scan.js](scan.js) illustrates using session.createQuery() to build and execute 
a [Query](../../database-jones/API-documentation/Query) that returns multiple records.
- [join.js](join.js) illustrates using a 
[Projection](../../database-jones/API-documentation/Projection) to define a 
relationship between tables, and then running session.find() against the 
projection.  The equivalent query in SQL would be a join.

The scan and join samples use the tables in the tweet demo, described below.


### The Tweet Demo

This directory contains a Twitter-like demo application using
Database Jones.

The SQL script create_tweet_tables.sql contains DDL statements for MySQL
to create the five tables used by the demo application.  It can be executed
using the standard mysql command:

    mysql -u root < create_tweet_tables.sql

The Node.js application [tweet.js](tweet.js) is a rather complete large example 
which can run as either as a command-line tool or as a REST web server.  tweet.js 
supplements the simple API examples with sample code for explicit transaction 
handling and batching (e.g. in InsertTweetOperation at line 320). *tweet.js*
responds to a number of verb-object commands; to see a list of them, simply
type:

    node tweet

Some demonstration scripts are provided to illustrate *tweet.js*:
- [demo_populate_data.sh](demo_populate_data.sh) populates the database with some sample data.
- [demo_cli_get.sh](demo_cli_get.sh) demonstrates querying the sample data from the command shell.
- [demo_http_get.sh](demo_http_get.sh) demonstrates querying the sample data (and posting a new
tweet) over the HTTP interface.
- [demo_http_delete.sh](demo_http_delete.sh) uses the HTTP interface to delete the sample data.

Note that in order to run the HTTP demo scripts, you should first start the
server on port 7800, using the command:

    node tweet start server 7800

tweet.js pays attention to the environment variables `JONES_ADAPTER` (which 
defaults to "ndb") and `JONES_DEPLOYMENT` (which defaults to "test").


### Connecting to a database server in your environment

Jones applications connect to a particular database using a
named **deployment** defined in the file 
[jones_deployments.js](../../jones_deployments.js).  The sample
code uses the "test" deployment. You can customize jones_deployments.js for
your environment, and you can supply a different deployment as a command-line
option in tweet.js or by editing the call to 
[ConnectionProperties()](../../database-jones/API-documentation/Jones) in the
API samples.


