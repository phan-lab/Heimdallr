#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "multisig.h"
#include <openssl/sha.h>

/*
char* multisig_config =
"type a\n"
"q 5606574725585475260156854876150694163481082876819940558885994420397771202275682787927182897520921796685619002987247979632157746700869606378558052121751871\n"
"h 3836174098186161700706781338150079224714246600555272825649298795894865749054149571162186493288081096134336\n"
"r 1461501637330902918203684832716283019653785059327\n"
"exp2 160\n"
"exp1 31\n"
"sign1 -1\n"
"sign0 -1";
char* multisig_g_val = "\x27\x0e\x1f\xc4\x83\xbb\xc6\xf8\x4e\x02\x41\x6a\xed\x67\xbe\xe1\x82\x0e\x1d\x06\x3e\x85\x1b\xd7\xdb\x81\x41\xc8\xf2\x5c\xd5\x9a\x43\x3d\x63\x39\x43\x19\x4f\xe5\x61\x80\xd0\x48\xe0\x36\x41\x7a\x8b\xfb\x7e\x44\x03\xd3\xd3\xb1\x70\x50\xd0\xf9\xb2\x98\xcc\xed\x10\x20\xa1\xd3\x27\xcb\x51\xf3\x81\x5b\x02\x73\xc6\x24\x58\x67\x49\x55\x8b\x15\x03\x05\xe0\xe2\xeb\x06\x24\x10\x82\xbb\x65\xbc\x54\xba\x44\xf4\xa7\x15\xd0\x6a\x9b\xb7\x59\x96\xe1\x53\x39\xbb\x25\x23\x92\xbd\x99\x00\x2f\x9b\xb6\xde\xad\x67\xa5\xe4\x10\x64";
*/
/*
// TYPE A Pairing
char* multisig_config = 
	"type a\n"
	"q 8780710799663312522437781984754049815806883199414208211028653399266475630880222957078625179422662221423155858769582317459277713367317481324925129998224791\n"
	"h 12016012264891146079388821366740534204802954401251311822919615131047207289359704531102844802183906537786776\n"
	"r 730750818665451621361119245571504901405976559617\n"
	"exp2 159\n"
	"exp1 107\n"
	"sign1 1\n"
	"sign0 1";
char* multisig_g_val = "\x02\x96\x44\x2b\xd5\x64\xaa\x54\x70\x57\xdc\x3d\x89\x3c\x64\x82\x32\x8c\xbb\x64\xad\xbc\xab\xca\x13\x0e\xf2\xf7\x40\x71\x7f\x05\x0d\x19\x06\x81\x62\x60\x34\x0a\x75\xa3\x54\x61\xb3\xd7\x4d\x62\x69\xe2\x96\xce\x83\xe4\x52\x38\x26\x00\xfd\xc1\x95\x22\x14\x64\x74\xa0\xb3\xca\x19\x29\x17\x79\x1b\x95\xbf\x80\x41\x46\x4e\x70\xf4\x23\xf7\x16\x0d\xcb\x14\x96\xe5\x49\x56\xc0\x89\xde\xcd\x24\xd0\x1d\x25\xdf\x26\x56\xf4\xf7\xcb\xf6\x1f\x32\xa5\x4a\x62\x39\x61\x6c\x4f\xb5\x74\x71\x36\xf7\xdf\xb8\x58\x13\x1f\x36\x0e\x3d";
*/
/*
// TYPE D
char* multisig_config = 
	"type d\n"
	"q 625852803282871856053922297323874661378036491717\n"
	"n 625852803282871856053923088432465995634661283063\n"
	"h 3\n"
	"r 208617601094290618684641029477488665211553761021\n"
	"a 581595782028432961150765424293919699975513269268\n"
	"b 517921465817243828776542439081147840953753552322\n"
	"k 6\n"
	"nk 60094290356408407130984161127310078516360031868417968262992864809623507269833854678414046779817844853757026858774966331434198257512457993293271849043664655146443229029069463392046837830267994222789160047337432075266619082657640364986415435746294498140589844832666082434658532589211525696\n"
	"hk 1380801711862212484403205699005242141541629761433899149236405232528956996854655261075303661691995273080620762287276051361446528504633283152278831183711301329765591450680250000592437612973269056\n"
	"coeff0 472731500571015189154958232321864199355792223347\n"
	"coeff1 352243926696145937581894994871017455453604730246\n"
	"coeff2 289113341693870057212775990719504267185772707305\n"
	"nqr 431211441436589568382088865288592347194866189652";

char* multisig_g_val = "\x62\x79\xe9\xf7\xf7\x51\x76\x9e\xab\xd3\xcf\x45\x09\x01\x95\x4f\x91\x11\x3f\x49\x4c\x27\x16\x0b\xef\xa9\x7a\x56\x99\x08\xe4\xef\x2c\x38\x13\x7c\xe4\xbd\xd0\xa5\x60\x03\x53\x2f\x8f\x7f\xfe\x70\x7c\xe0\x93\xa4\x64\x51\x8c\x1a\x90\xe9\x3e\x4b\x36\x47\x86\xb2\xa5\x8f\xe9\x5d\xd5\x08\xb7\x83\xfe\x73\x81\x68\x7c\x4e\x4f\x3b\x0d\x55\x76\xb5\x98\x8a\xcb\x3f\x52\x75\x53\x28\x27\x71\x30\x2d\x31\x3a\x8e\x00\x3c\xe0\x42\x86\xd5\x45\x2c\xfb\x9b\xf0\x66\xaa\x55\x52\xbf\xf3\x15\xc8\x06\x36";

*/
/*
// TYPE A1 (p-192 NIST)
char* multisig_config = 
"type a1\n"
"p 6277101735386680763835789423207666416083908700390324961279\n"
"n 6277101735386680763835789423176059013767194773182842284081\n"
"l 1";
char* multisig_g_val = "\x83\x11\xd3\xf9\x7a\x7d\xa1\x03\xe8\xed\xc2\x36\x95\x7a\x5c\x5a\x8b\x2a\xc8\x8e\x0f\x6b\x69\x22\x14\x16\xf0\x74\xa9\x12\x48\xb1\x11\x57\x35\x03\xdb\xb5\xde\x15\x74\x94\x8a\xb5\x43\x65\x5a\x9e";
*/

/*
// Brian TYPE A, r 100 bits, base field 256 bits 
// 1165.3 us to verify avg
char* multisig_config = 
"type a\n"
"q 2924073583359088344465805690809127093673918542553410564207009943218043260571\n"
"h 2306696133248930285660190082407495665597772132\n"
"r 1267645764524950942980004380671\n"
"exp2 100\n"
"exp1 82\n"
"sign1 -1\n"
"sign0 -1";
char* multisig_g_val = "\x04\x80\x8f\x37\x02\xb9\x9d\x00\x06\x00\x24\x5e\x97\x83\x5a\xce\xa5\x07\x08\xfa\x26\xba\xd5\x6e\x6a\x92\x92\x6a\xf4\xb1\xc3\x21\x00\xc0\x9a\xe6\x47\x9f\xe3\xe3\x53\xf5\x47\xd1\x5e\x8a\x1a\xfb\xb9\xb7\x7a\x49\x6b\xf5\x81\x20\x2e\xd6\x40\xd9\x75\xd3\x8d\x16";
*/
/*
// BRIAN GOTO for fast
// BRIAN r 100, base field 170 bits
char* multisig_config =
"type a\n"
"q 1572512633725304471225711721672511393778695766599859\n"
"h 1240493739712021063860\n"
"r 1267650600228229400397191577601\n"
"exp2 100\n"
"exp1 40\n"
"sign1 -1\n"
"sign0 1";
char* multisig_g_val = "\x02\xe8\xa7\xc3\xf3\xb9\x79\xc5\xc2\x96\xe7\xa4\x7d\xc0\x40\xe0\x48\x00\xc8\x3b\x2c\xae\x02\x23\x7f\xd0\xe7\xad\xf9\x7d\x85\x43\x74\x85\x52\x38\x2d\x51\x19\xeb\xd2\x4f\x8b\xb8";
*/
// Testing this one
// r = 120, q = 256
/*
char* multisig_config =
"type a\n"
"q 34355802764879350973099081720254726503274626008278774059964328740331803518767\n"
"h 25846433323594965075987084156362520910032\n"
"r 1329227995783706947084192431105638399\n"
"exp2 120\n"
"exp1 80\n"
"sign1 -1\n"
"sign0 -1";

char* multisig_g_val = "\x1d\xcc\x7c\xb4\x37\x50\xf2\x3d\xa2\xf0\xd4\xd4\x86\x4f\x83\x57\xf2\xeb\x32\x94\x1a\x4b\x0a\x5b\x90\x2f\xdc\x63\x3e\xf7\xdd\x5d\x2a\xd7\xf8\x4b\x12\xd7\x45\xf9\x9f\x2e\x9d\x1a\x06\xcf\x3b\x63\x4f\xc7\x19\xd8\x62\x27\x5b\xd2\x29\xf3\x3f\x61\x1e\x1e\xca\x04";
*/
// Brian LOW LOW r 50 q 150
/*
char* multisig_config = 
"type a\n"
"q 401866727\n"
"h 24\n"
"r 16744447\n"
"exp2 24\n"
"exp1 15\n"
"sign1 -1\n"
"sign0 -1";
char* multisig_g_val = "\x0d\xd1\x77\xc2\x01\xd9\xaa\x29";
*/

/*
// rbits = 154, qbits = 256
char* multisig_config =
"type a\n"
"q 38376935826529746154920156510859470989632209182798574237655833448048034318123\n"
"h 3361096327450511726462553294636\n"
"r 11417981541647679048466287755595978683248017409\n"
"exp2 153\n"
"exp1 44\n"
"sign1 1\n"
"sign0 1";
*/
char multisig_config[] =
"type a\n"
"q 46715609578734551849756403761647028741420588238763613510048226706256089815383\n"
"h 63928234338553166055247157592\n"
"r 730750818665451459101842416358717970580269694977\n"
"exp2 159\n"
"exp1 59\n"
"sign1 1\n"
"sign0 1";

char multisig_g_val[] = "\x49\x63\x24\xf6\xc9\xd0\xa6\x01\x60\x16\x47\x46\xf1\x36\xd4\x2e\xa2\x1a\xb1\xdf\xe5\xd5\x83\x30\x4f\xf4\x56\x45\x0d\xa9\x33\x25\x15\x8b\x65\xce\x46\x2f\x3b\x72\x17\xeb\x91\xaa\x4c\x72\x27\x3e\xe5\x61\x76\x56\x49\xa3\x06\x4c\xa9\x31\xf9\x64\xb4\x7c\x66\x2b";

element_t multisig_g;
pairing_t multisig_pairing;
//pairing_pp_t multisig_pairing_pp_g;
int multisig_loaded = 0;

// hash should be of length SHA256_DIGEST_LENGTH
void sha256_hash(char* msg, int len, char* hash) {
	// SHA256_CTX ctx;
	// SHA256_Init(&ctx);
	// SHA256_Update(&ctx, msg, len);
	// SHA256_Final((unsigned char*) hash, &ctx);
	SHA256((unsigned char*)msg, (size_t)len, (unsigned char*) hash);
}

void multisig_gen_g() {
	// Load the configuration from the string.
	pairing_init_set_buf(multisig_pairing, multisig_config, strlen(multisig_config));
	// Select a random g.
	element_t g;
	element_init_G2(g, multisig_pairing);
	element_random(g);
	int len = element_length_in_bytes(g);
	char buf[len];
	element_to_bytes((unsigned char*) buf, g);
	printf("Please include the following line in multisig.c:\n\n");
	printf("char* multisig_g_val = \"");
	for (int i = 0 ; i < len; i++) {
		unsigned char ch = (unsigned char) buf[i];
		printf("\\x%02x", ch);
	}
	printf("\";\n");
}

void multisig_init() {
	if(multisig_loaded) return;
	// Load the configuration from the string.
	pairing_init_set_buf(multisig_pairing, multisig_config, strlen(multisig_config));
	// Load g from config string.
	element_init_G2(multisig_g, multisig_pairing);
	element_from_bytes(multisig_g, (unsigned char*) multisig_g_val);
	//pairing_pp_init(multisig_pairing_pp_g, multisig_g, multisig_pairing);
	multisig_loaded = 1;
}
// void multisig_init(pairing_t pair) {
// 	if(multisig_loaded) return;
// 	// Load the configuration from the string.
// 	pairing_init_set_buf(pair, multisig_config, strlen(multisig_config));
// 	// Load g from config string.
// 	element_init_G2(multisig_g, pair);
// 	element_from_bytes(multisig_g, (unsigned char*) multisig_g_val);
// 	//pairing_pp_init(multisig_pairing_pp_g, multisig_g, pair);
// 	multisig_loaded = 1;
// }
// 
void multisig_gen_keypair(multisig_t* public_key, multisig_t* secret_key) {
	element_t* pk = &(public_key->el);
	element_t* sk = &(secret_key->el);
	// Set domain of each.
	element_init_G2(*pk, multisig_pairing);
	element_init_Zr(*sk, multisig_pairing);
	// x = random number from Zr
	element_random(*sk);
	// y = g^x
	element_pow_zn(*pk, multisig_g, *sk);
}
void multisig_sign(multisig_t* sig, char* buf, int len, multisig_t* secret_key) {
	/* Hash the buffer contents. */
	element_t h;
	element_init_G1(h, multisig_pairing);
	char hash[SHA256_DIGEST_LENGTH];
	sha256_hash(buf, len, hash);
	element_from_hash(h, hash, SHA256_DIGEST_LENGTH);

	/* Initialize sig variable to G1 */
	element_init_G1(sig->el, multisig_pairing);

	/* sig = h^secret_key */
	element_pow_zn(sig->el, h, secret_key->el);

	/* Don't want a mem leak! */
	element_clear(h);
}

int multisig_verify(char* buf, int len, multisig_t* public_key, multisig_t* sig) {
	element_t h;
	element_init_G1(h, multisig_pairing);

	char hash[SHA256_DIGEST_LENGTH];
	sha256_hash(buf, len, hash);
	element_from_hash(h, hash, SHA256_DIGEST_LENGTH);

	element_t temp1, temp2;
	element_init_GT(temp1, multisig_pairing);
	element_init_GT(temp2, multisig_pairing);

	pairing_apply(temp1, sig->el, multisig_g, multisig_pairing);
	//pairing_pp_apply(temp1, sig->el, multisig_pairing_pp_g);
	pairing_apply(temp2, h, public_key->el, multisig_pairing);

	if (!element_cmp(temp1, temp2)) {
		element_clear(h);
		element_clear(temp1);
		element_clear(temp2);
		return 1; //Verified!
	}
	element_clear(h);
	element_clear(temp1);
	element_clear(temp2);
	return 0;
}

void multisig_combine_helper(multisig_t* out, multisig_t* items, unsigned int num_items) {
	if (num_items == 0) return;
	element_set(out->el, items[0].el);
	for (unsigned int i = 1; i < num_items; i++) {
		element_mul(out->el, out->el, items[i].el);
	}
}

void multisig_combine_sig(multisig_t* out, multisig_t* sig1, multisig_t* sig2) {
	element_mul(out->el, sig1->el, sig2->el);
}
void multisig_combine_key(multisig_t* out, multisig_t* key1, multisig_t* key2) {
	element_mul(out->el, key1->el, key2->el);
}
void multisig_combine_sigs(multisig_t* out, multisig_t* sigs, unsigned int num_sigs) {
	element_init_G1(out->el, multisig_pairing);
	multisig_combine_helper(out, sigs, num_sigs);
}

void multisig_combine_keys(multisig_t* out, multisig_t* keys, unsigned int num_keys) {
	element_init_G2(out->el, multisig_pairing);
	multisig_combine_helper(out, keys, num_keys);
}

void multisig_print(multisig_t* ms) {
	element_fprintf(stderr, "%B", ms->el);
}
void multisig_set(multisig_t* dst, multisig_t* src) {
	element_init_same_as(dst->el, src->el);
	element_set(dst->el, src->el);
}
void multisig_to_buf(multisig_t* ms, char* buf) {
#ifdef MULTISIG_LENGTH_CHECK
	int num_bytes = element_length_in_bytes(ms->el);
	if (num_bytes > 128) {
		printf("ERROR: Need to update MULTISIG_LENGTH. Requires %d bytes.\n", num_bytes);
		exit(1);
	}
#endif
	element_to_bytes((unsigned char*) buf, ms->el);
}
void multisig_from_buf(multisig_t* ms, char* buf) {
	element_from_bytes(ms->el, (unsigned char*) buf);
}
void multisig_init_pub(multisig_t* ms) {
	element_init_G2(ms->el, multisig_pairing);
}
void multisig_init_sec(multisig_t* ms) {
	element_init_Zr(ms->el, multisig_pairing);
}
void multisig_init_sig(multisig_t* ms) {
	element_init_G1(ms->el, multisig_pairing);
}
