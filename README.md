# med_pill
Medicine dispenser (pill dispenser) automated by arduino "ESP32", with wi-fi connection, alerts via Telegram and Alexa 


# This project aims to meet the needs of users who have difficulties in taking medication at predetermined times. 

Like elderly people. The MED_PILL execution line is to deliver the medication automatically and notify the patient through an audible / light signal and message via Alexa that the medication is available to be taken. 

It also informs the caregiver or a relative that the patient has taken the medicine at the moment he takes the medicine out of the compartment. 

The alert will be sent through Telegram in this first version. 

# The project

The project is divided into three parts. 

    • 3D Printing 
      
    • Applications and Webhooks. 
      
    • Programming.
      
# 1. 3D Printing 

The pieces to be printed in 3D were based on a project made by Kamil Sopak. 

To access the original model access the link " https://www.thingiverse.com/thing:1291374 " 

Some changes to the original design were necessary to meet the new design needs. 

The piece has 21 compartments for pills, which can be administered once, twice or three times a day, according to the schedule to be made.


# 2. Aplicativos e Webhooks.

    • Telegram ( @botfather ) ;

Open a chat with @BotFather in Telegram and click the /start command.

After you get a list of commands Select the command /newbot to get this Message:

Alright, a new bot. How are we going to call it? Please choose a name for your bot.

Enter a name for your bot, which can be anything, and send it. After that BotFather will ask for a username for your bot:

Good. Now let's choose a username for your bot. It must end in bot. Like this, for example: mom_pill_bot or  dad_pill_bot.

That must end with “bot”. If your desired username is taken or not available, try again with another name.

Finally your bot is created and the following message is shown. You must use the bot token to communicate with the Telegram, so make sure you write it down.

Done! Congratulations on your new bot. 

You will find it at telegram.me/???bot.

You can now add a description, about section and profile picture for your bot, see /help for a list of commands. 

By the way, when you've finished creating your cool bot, ping our Bot Support if you want a better username for it. 

Just make sure the bot is fully operational before you do this.

Use this token to access the HTTP API: xxx:xxx

With the gained token you now can send a test message by calling the website https://api.telegram.org/botBOTTOKEN/sendmessage?chat_id=YOURCHATID&text=YOURTEXT

    • IFTTT, Webhook, MkZense, Alexa 

Attention: 

This feature is only possible if you have an Amazon account and Echo Dot device. 

Even without Echo-Dot MED_PILL will only work with Telegram alerts. 

If you need another type of personal assistant, use the other existing integrations in IFTTT. 

The integration of these three services aims to generate a more playful alert to the patient, informing the time to take the medicine. 

In addition to the possibility of adding an incentive message. 

The basis of this alert will be the IFTTT service. 

Please create an account on this service, it is free for up to 3 types of activation. 

Link: https://ifttt.com 


After creating the account, add the existing connection service in IFTTT. - mkzense -MKZENSE ( trigger for alexa ) 

When accessing this feature on IFTTT, you must release MZZense access to your Amazon account, the release will be made automatically for Skills. 

  - Webhook
  
This service will trigger the private trigger registered in MKZENSE to execute a Skill on Alexa. 

  - Skill Creation on Alexa. 
  
After creating the trigger in IFTTT, we must create the skill that will be activated during drug delivery. 

The videos for creating skills can be seen on the website, https://mkzense.com/


# 3. Programming.

The program was carried out in C++, as it is a simpler method for recording data on the Arduino. 

Nothing prevents being transmuted to python language. 

The board is based on ESP32, in this experiment we are using the Wemos D1 R32.
