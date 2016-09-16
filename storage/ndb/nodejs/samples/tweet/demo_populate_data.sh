# This script populates the sample database
export JONES_ADAPTER=mysql || setenv JONES_ADAPTER mysql  # sh || csh
set -x

# Create four users: Caligula, Claudius, Nero, Agrippina
#
node tweet put user caligula '{ "full_name": "Gaius Julius Caesar Germanicus" , "profile_text": "I am your little boot!" }'
node tweet put user uncle_claudius '{ "full_name": "Tiberius Claudius Nero Germanicus" }'
node tweet put user nero '{ "full_name": "Lucius Domitius Ahenobarus" }'
node tweet put user agrippina '{ "full_name": "Julia Augusta Agrippina Minor"}'


# Create follow records: Nero follows Agrippina, etc.
#
node tweet put follow nero agrippina
node tweet put follow agrippina nero
node tweet put follow agrippina uncle_claudius
node tweet put follow agrippina caligula


# Now post some tweets from each user
#
node tweet post tweet caligula '@agrippina You really are my favorite sister.'
node tweet post tweet agrippina '@nero Remember to be nice to Uncle Claudius!' 
node tweet post tweet nero 'I love to sing!'
node tweet post tweet nero 'I am the best #poet and the best #gladiator!'
node tweet post tweet agrippina \
 '@uncle_claudius Please come over for dinner, we have some fantastic #mushrooms'
node tweet post tweet uncle_claudius 'I am writing a new history of #carthage'
node tweet post tweet caligula '@agrippina you are my worst sister! worst!' 
node tweet post tweet caligula '@agrippina Rome is terrible!!!'
