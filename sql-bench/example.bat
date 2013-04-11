REM
REM Example use to run test on PC
REM One need only to run 'comapre-results' to view results from old runs

perl run-all-tests --server mysql --cmp "access,mysql"
perl run-all-tests --server mysql --cmp "access,mysql" --log --use-old-results
perl compare-results --cmp  "access,mysql" -rel
