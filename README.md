Crypto1Final-Bank
=================
##Protocol
All transmissions use one block of AES256 encrypted data. The unencrypted specification is in spec.txt.
The ATM sends a request to the bank, which processes the transaction and sends a response. Out of order packets are not accepted.
To force disconnect from the server, send a packet with a transaction number of 255 and a message of "disc".
##Usage
####Setup
Make sure your account has an atm card. The cards are included.
####ATM usage
Fill in each field, using space or tab to go to the next field.
If a field is not valid, you won't be able to advance.
Press enter to submit form.
