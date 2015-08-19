TSF is a columnar chunked and compressed storage format that uses SQLite as a container.

It supports matrix and attribute fields, and is optimized for genomic interval queries (using a tabix-like built-in internal index).

See:
http://blog.goldenhelix.com/grudy/all-i-want-for-christmas-is-a-new-file-format-for-genomics/

Currently the only "writer" for TSF files is Golden Helix's products VarSeq and GenomeBrowse:

http://genomebrowse.com

http://varseq.com

Note all public genomic annotations easily downloadable form VarSeq/GenomeBrowse are TSF files, and it scales quite nicely.

For example, the dbNSFP functional prediction database of ~90 million predictions of 5 algorithm plus gene and variant meta-data (15 string fields) is only ~435MB. A single float field for ~10 million records compresses to about ~20MB.

This an efficient C reader implementation, that supports reading TSF in other contexts (such as in a PostgreSQL Foreign Data Wrapper). 

Email Gabe Rudy <rudy@goldenhelix.com> with any questions.
