BCHFILE-uploader
================

This is a tool for uploading files to the BCH/BSV/BTC blockchain.

How to Run:
-----------

Download the code:

$ git clone https://github.com/bchfile/BCHFILE-uploader.git

$ cd BCHFILE-uploader

Compile it:

$ cc bchfile_uploader.c -o bchfile

$ cc sendtx.c -o sendtx

Prepare a file named 'bchfile_key' which including the address and privateKey for sign transactions.

Run it:

$ ./bchfile your_filename txid_src vout_src balance_in

The program will save all transactions to a file 'upload_TXs', then run sendtx to broadcast all TXs to the network.

$ ./sendtx

For testnet
-----------
Uncomment '#define TESTNET' and recompile.

For BTC
-----------
Uncomment '#define BTC' and recompile.

this program is not tested on BTC network yet.

Hints
-----
You need to prepare sufficient balance for uploading, a suggestion is 6*file_size*FEE satoshis(assume the fee is FEE/bytes, FEE = 1 by default).

The storage efficiency(file_size/all_transactions_size) is about 34%.