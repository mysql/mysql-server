# Some sample queries using the command line
export JONES_ADAPTER=mysql || setenv JONES_ADAPTER mysql  # sh || csh


# Get tweets that mention @agrippina
echo "Getting tweets @agrippina:"
node tweet get tweets-at agrippina

# Get tweets with hashtag #carthage
echo "Getting tweets about #carthage:"
node tweet get tweets-about carthage

# Get tweets written by Nero
echo "Getting tweets by nero:"
node tweet get tweets-by nero

# Get the five most recent tweets
echo "Getting the five most recent tweets:"
node tweet get tweets-recent 5

# Nobody follows Claudius
echo "Getting: who follows claudius?"
node tweet get followers uncle_claudius

# See who Agrippina follows
echo "Getting: who does agrippina follow?"
node tweet get following agrippina
