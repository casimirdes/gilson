/*
 * gilson.h
 *
 *  Created on: 30 de out. de 2024
 *      Author: mella
 */



/*
 ============================================================================
 Name			: gilson_c
 Author			: matheus j. mella
 Version		: 0.55
 Date			: 12/10/25
 Description	: biblioteca 'gilson'
 GitHub			: https://github.com/casimirdes/gilson
 ============================================================================
 */



#ifndef GILSON_H_
#define GILSON_H_

#define TESTE_GSON_MODO_JSON 	0  // ainda experimental...

#define CAST_GIL	(uint8_t *)&  // forma bem deselegante de padronizar para fazer o 'casting'

#define LIMIT_GSON_KEYS		255  // até 255 chaves cada pacote gilson, (0 a 254)

enum e_MODO_GILSON
{
	GSON_MODO_ZIP,  	// é cru, sem nada, somente a data bruta, tem 1 byte adicional que é [0]=modo
	GSON_MODO_FULL,  	// modo padão com offset, crc, identificador
	GSON_MODO_KV,  		// modo GSON_MODO_FULL + KV ala key:value chave:valor salva nome em string das chaves ala JSON da vida
	GSON_MODO_KV_ZIP,  	// modo GSON_MODO_ZIP porem salva os nomes das chaves ala JSON da vida
	GSON_MODO_JSON,		// caso particular de encode/decode de um JSON estático *** só usa com funcoes "xxxxx_json" experimental...
	GSON_MODO_MAX
};

enum e_TIPOS1_GILSON
{
	GSON_SINGLE,  		// valor unico
	GSON_LIST,  		// é no formato lista, [u16] mas até 64k
	GSON_MTX2D,  		// é no formato matriz, max 2 dimenções!!! [u8][u16] mas jamais pode passar de 64k!!!!
	GSON_MAX
	// GSON_MAX == 0b111, 0 a 7
};

enum e_TIPOS2_GILSON
{
	// reserva o '0' para outros usos...
	GSON_tBIT=1,
	GSON_tINT8,
	GSON_tUINT8,
	GSON_tINT16,
	GSON_tUINT16,
	GSON_tINT32,
	GSON_tUINT32,
	GSON_tINT64,
	GSON_tUINT64,
	GSON_tFLOAT32,
	GSON_tFLOAT64,
	GSON_tSTRING,
	GSON_tMAX,
	// GSON_tMAX == 0b1110 (1 a 30)
};


enum e_erros_GILSON
{
	// vai fazer 'erGSON_xxxx' onde xxxx-1000 = erro retornado
	erGSON_OK = 0,
	erGSON_NULL = -1000,  		// add chave 'null' no modo ZIP
	erGSON_OPER,				// erro de operação, está em enconde e quer usar funcao de decode ou contrário
	erGSON_DIFKEY,  			// validando decode, total de chaves mapa diferentes com pacote
	erGSON_DIFKEYb,  			// validar pacote que não é FULL
	erGSON_eJSON,  				// está querendo encodar 'GSON_MODO_JSON' no encode errado
	erGSON_dJSON,  				// está querendo decoder 'GSON_MODO_JSON' no decode errado
	erGSON_LIMKEY,  			// limite de chaves, atual = 256 (0 a 255)
	erGSON_SMKEYe,  			// chave ja foi adicionada no encode
	erGSON_SMKEYd,  			// chave ja foi adicionada no decode
	erGSON_0,  					// gilson_encode_init: já tem um pacote ativo
	erGSON_1,  					// gilson_encode_init: 2 pacotes ativos, não é possível iniciar
	erGSON_2,  					// gilson_encode_init: quer utililzar o mesmo buffer geral na RAM????? não tem fazer isso...
	erGSON_3,  					// gilson_encode_data_base: não chamou init encode
	erGSON_4,  					// gilson_encode_data_base: erro tipo1
	erGSON_4b,  				// gilson_encode_data_base: erro tipo2
	erGSON_5,  					// gilson_encode_data_base: quer add uma chave maior do que a contagem crescente... não tem como
	erGSON_6,  					// explodiu o buffer de entrada
	erGSON_7,  					// no modo KV, tamanho do nome da chave é maior que LEN_MAX_CHAVE_NOME
	erGSON_8,  					// tamanho da lista está zerado cont_list_a==0
	erGSON_9,  					// lista de string sem definir tamanho max da string
	erGSON_9b,  				// tamanho da string passa do limite permitido (>= LEN_MAX_STRING)
	erGSON_10,  				// matriz de string não é permitido
	erGSON_11,  				// matriz com algum cont_list_X zerado
	erGSON_12,  				// lista de string sem definir tamanho max da string
	erGSON_12b,  				// tamanho da string passa do limite permitido (>= LEN_MAX_STRING)
	erGSON_13,  				// gilson_encode_data_base: tipo2 inválido
	erGSON_14,  				// string ficou maior que LEN_MAX_STRING
	erGSON_15,  				// gilson_decode_init: quer decodificar e já está com pacotes ativos
	erGSON_16,  				// gilson_decode_init: quer decodificar e já está com os 2 pacotes ativos
	erGSON_17,  				// gilson_decode_init: quer utililzar o mesmo buffer geral????? não tem fazer isso....
	erGSON_18,  				// gilson_decode_init: no decode, pacote com erro de 'GSON_MODO_MAX'
	erGSON_19,  				// gilson_decode_init: erro valida crc pacote FULL
	erGSON_20,  				// gilson_decode_end_base: chegou até o fim 'cont_itens2' mas o 'pos_bytes' não bateu com o 'pos_bytes2', dai deu ruim
	erGSON_21,  				// gilson_decode_data_base: pacote não está ativo
	erGSON_22,  				// gilson_decode_data_base: tipo1 inválido
	erGSON_22b,  				// gilson_decode_data_base: tipo2 inválido
	erGSON_23,  				// gilson_decode_data_base: quer ler chave maior fora da ordem crescente, ZIP/FULL
	erGSON_23b,  				// gilson_decode_data_base: quer ler chave que não existe, maior que programado, ZIP/FULL
	erGSON_24,  				// gilson_decode_data_base: lista com 'cont_list_a' zerado
	erGSON_25,  				// gilson_decode_data_base: lista de strings com 'cont_list_b' zerado
	erGSON_26,  				// gilson_decode_data_base: matriz com algum 'cont_list_X' zerado
	erGSON_DIFIN,  				// gilson_decode_data_base: no decode, diferenças de paramentros de entrada no modo FULL
	erGSON_27,  				// gilson_decode_data_base: tipo2 inválido
	erGSON_28,  				// gilson_decode_data_full_base: pacote não está ativo
	erGSON_29,  				// gilson_decode_data_full_base: modo incompatível, tem que ser FULL
	erGSON_30,  				// gilson_decode_data_full_base: quer ler uma chave que não existe, modo FULL, tipo 'erGSON_23b'
	erGSON_31,  				// gilson_decode_data_full_base: tipo1 inválido
	erGSON_31b,  				// gilson_decode_data_full_base: tipo2 inválido
	erGSON_32,  				// gilson_decode_data_full_base: lista com 'cont_list_a' zerado
	erGSON_33,  				// gilson_decode_data_full_base: lista de strings com 'cont_list_b' zerado
	erGSON_34,  				// gilson_decode_data_full_base: matriz com algum 'cont_list_X' zerado
	erGSON_35,  				// gilson_decode_data_full_base: tipo2 inválido
	erGSON_36,  				// gilson_encode_dl_init: pacote não está ativo
	erGSON_37,  				// gilson_encode_dl_init: ja está ativo esse modo... tem que teminar antes para iniciar um novo
	erGSON_38,  				// gilson_encode_dl_init: quer add uma chave maior do que a contagem crescente... não tem como
	erGSON_39,  				// valid_gilson_encode_dl: pacote não está ativo
	erGSON_40,  				// valid_gilson_encode_dl: tipo não é dinâmico
	erGSON_41,  				// valid_gilson_encode_dl: tipo1 inválido
	erGSON_41b,  				// valid_gilson_encode_dl: tipo2 inválido
	erGSON_42,  				// valid_gilson_encode_dl: quer add uma item maior do que a contagem crescente
	erGSON_43,  				// valid_gilson_encode_dl: lista com 'cont_list_a' zerado
	erGSON_44,  				// valid_gilson_encode_dl: lista de string com 'cont_list_b' zerado
	erGSON_45,  				// valid_gilson_encode_dl: matriz de string não pode
	erGSON_46,  				// valid_gilson_encode_dl: matriz com algum 'cont_list_X' zerado
	erGSON_47,  				// valid_gilson_encode_dl: lista de string sem definir tamanho max da string
	erGSON_48,  				// gilson_encode_dl_end: pacote não é tipo dinâmico
	erGSON_49,  				// gilson_encode_dl_end: total de itens geral não bate a conta com o programado
	erGSON_50,  				// gilson_decode_dl_init: pacote não está ativo
	erGSON_51,  				// gilson_decode_dl_init: ja está ativo esse modo... tem que teminar antes para iniciar um novo
	erGSON_52,  				// gilson_decode_dl_init: quer ler uma chave maior do que a contagem programada
	erGSON_53,  				// gilson_decode_dl_init: identificador 'TIPO_GSON_LDIN' inválido
	erGSON_54,  				// gilson_decode_dl_init: tipo1 inválido
	erGSON_54b,  				// gilson_decode_dl_init: tipo2 inválido
	erGSON_55,  				// gilson_decode_dl_init: lista com 'cont_list_a' zerado
	erGSON_56,  				// gilson_decode_dl_init: lista de string com 'cont_list_b' zerado
	erGSON_57,  				// gilson_decode_dl_init: matriz com algum 'cont_list_X' zerado
	erGSON_58,  				// gilson_decode_dl_data: pacote não é modo FULL
	erGSON_59,  				// gilson_decode_dl_data: quer ler uma item maior do que a contagem crescente... não tem como
	erGSON_60,  				// gilson_decode_dl_end: pacote não é tipo dinâmico
	erGSON_61,  				// gilson_decode_dl_end: total de itens geral não bate a conta com o programado
	erGSON_62,  				// gilson_decode_key: pacote não é modo FULL
	erGSON_63,  				// gilson_encode_dl_init: pacote não é modo FULL
	erGSON_64,  				//
	erGSON_65,  				//
	erGSON_66,  				//
	erGSON_67,  				//
	erGSON_68,  				//
	erGSON_69,  				//
	erGSON_70,  				//
	erGSON_71,  				//
	erGSON_72,  				//
	erGSON_73,  				//
	erGSON_74,  				//
};


// parte de encode ======================================================
int32_t gilson_encode_init(const uint8_t modo_, uint8_t *pack, const uint16_t size_max_pack);
int32_t gilson_encode_data(const uint8_t chave, const uint8_t tipo1, const uint8_t tipo2, const uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_encode_dataKV(const uint8_t chave, const uint8_t tipo1, const uint8_t tipo2, char *nome_chave, const uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_encode(const uint8_t chave, const uint8_t tipo1, const uint8_t tipo2, ...);
int32_t gilson_encode_mapfix(const uint16_t *map, ...);
int32_t gilson_encode_mapdin(const uint16_t *map, ...);
int32_t gilson_encode_data_null(const uint8_t chave, ...);
int32_t gilson_encode_end(void);
int32_t gilson_encode_end_crc(uint32_t *crc);

// encode lista dinâmica "dl" SOMENTE no modo FULL
int32_t gilson_encode_dl_init(const uint8_t chave, const uint8_t tam_list, const uint8_t nitens);
int32_t gilson_encode_dl_add(const uint8_t item, const uint8_t tipo1, const uint8_t tipo2, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_encode_dl_data(const uint8_t item, const uint8_t tipo1, const uint8_t tipo2, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_encode_dl_end(void);

// parte de decode ======================================================
int32_t gilson_decode_init(const uint8_t *pack, uint8_t *modo);
int32_t gilson_decode_valid(const uint8_t *pack);
int32_t gilson_decode_data(const uint8_t chave, const uint8_t tipo1, const uint8_t tipo2, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_decode_dataKV(const uint8_t chave, char *nome_chave, const uint8_t tipo1, const uint8_t tipo2, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_decode_data_full(const uint8_t chave, uint8_t *valor);
int32_t gilson_decode_dataKV_full(const uint8_t chave, char *nome_chave, uint8_t *valor);
int32_t gilson_decode(const uint8_t chave, ...);
int32_t gilson_decode_mapfix(const uint16_t *map, ...);
int32_t gilson_decode_mapdin(const uint16_t *map, ...);
int32_t gilson_decode_end(void);
int32_t gilson_decode_end_crc(uint32_t *crc);

int32_t gilson_decode_valid_map(const uint16_t map_full[][6], const uint16_t tot_chaves, const uint8_t *pack);

int32_t gilson_decode_key(const uint8_t *pack, const uint8_t chave, uint8_t *valor);

// decode lista dinâmica "dl"
int32_t gilson_decode_dl_init(const uint8_t chave);
//int32_t gilson_decode_dl_data_zip(const uint8_t item, const uint8_t tipo1, const uint8_t tipo2, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_decode_dl_data(const uint8_t item, uint8_t *valor);
int32_t gilson_decode_dl_end(void);


// funcoes auxiliares fortemente tipadas de encode
//========================================= GSON_SINGLE
int32_t gilson_encode_u8(const uint8_t chave, const uint8_t valor);
int32_t gilson_encode_s8(const uint8_t chave, const int8_t valor);
int32_t gilson_encode_u16(const uint8_t chave, const uint16_t valor);
int32_t gilson_encode_s16(const uint8_t chave, const int16_t valor);
int32_t gilson_encode_u32(const uint8_t chave, const uint32_t valor);
int32_t gilson_encode_s32(const uint8_t chave, const int32_t valor);
int32_t gilson_encode_u64(const uint8_t chave, const uint64_t valor);
int32_t gilson_encode_s64(const uint8_t chave, const int64_t valor);
int32_t gilson_encode_f32(const uint8_t chave, const float valor);
int32_t gilson_encode_f64(const uint8_t chave, const double valor);
int32_t gilson_encode_str(const uint8_t chave, const char *valor, const uint16_t cont_list_a);

//========================================= GSON_LIST
int32_t gilson_encode_lu8(const uint8_t chave, const uint8_t valor[], const uint16_t cont_list_a);
int32_t gilson_encode_ls8(const uint8_t chave, const int8_t valor[], const uint16_t cont_list_a);
int32_t gilson_encode_lu16(const uint8_t chave, const uint16_t valor[], const uint16_t cont_list_a);
int32_t gilson_encode_ls16(const uint8_t chave, const int16_t valor[], const uint16_t cont_list_a);
int32_t gilson_encode_lu32(const uint8_t chave, const uint32_t valor[], const uint16_t cont_list_a);
int32_t gilson_encode_ls32(const uint8_t chave, const int32_t valor[], const uint16_t cont_list_a);
int32_t gilson_encode_lu64(const uint8_t chave, const uint64_t valor[], const uint16_t cont_list_a);
int32_t gilson_encode_ls64(const uint8_t chave, const int64_t valor[], const uint16_t cont_list_a);
int32_t gilson_encode_lf32(const uint8_t chave, const float valor[], const uint16_t cont_list_a);
int32_t gilson_encode_lf64(const uint8_t chave, const double valor[], const uint16_t cont_list_a);
int32_t gilson_encode_lstr(const uint8_t chave, const char *valor, const uint16_t cont_list_a, const uint16_t cont_list_b);

//========================================= GSON_MTX2D
int32_t gilson_encode_mu8(const uint8_t chave, const uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_encode_ms8(const uint8_t chave, const int8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_encode_mu16(const uint8_t chave, const uint16_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_encode_ms16(const uint8_t chave, const int16_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_encode_mu32(const uint8_t chave, const uint32_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_encode_ms32(const uint8_t chave, const int32_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_encode_mu64(const uint8_t chave, const uint64_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_encode_ms64(const uint8_t chave, const int64_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_encode_mf32(const uint8_t chave, const float *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_encode_mf64(const uint8_t chave, const double *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);


// funcoes auxiliares fortemente tipadas de decode
//========================================= GSON_SINGLE
int32_t gilson_decode_u8(const uint8_t chave, uint8_t *valor);
int32_t gilson_decode_s8(const uint8_t chave, int8_t *valor);
int32_t gilson_decode_u16(const uint8_t chave, uint16_t *valor);
int32_t gilson_decode_s16(const uint8_t chave, int16_t *valor);
int32_t gilson_decode_u32(const uint8_t chave, uint32_t *valor);
int32_t gilson_decode_s32(const uint8_t chave, int32_t *valor);
int32_t gilson_decode_u64(const uint8_t chave, uint64_t *valor);
int32_t gilson_decode_s64(const uint8_t chave, int64_t *valor);
int32_t gilson_decode_f32(const uint8_t chave, float *valor);
int32_t gilson_decode_f64(const uint8_t chave, double *valor);
int32_t gilson_decode_str(const uint8_t chave, char *valor);

//========================================= GSON_LIST
int32_t gilson_decode_lu8(const uint8_t chave, uint8_t valor[], const uint16_t cont_list_a);
int32_t gilson_decode_ls8(const uint8_t chave, int8_t valor[], const uint16_t cont_list_a);
int32_t gilson_decode_lu16(const uint8_t chave, uint16_t valor[], const uint16_t cont_list_a);
int32_t gilson_decode_ls16(const uint8_t chave, int16_t valor[], const uint16_t cont_list_a);
int32_t gilson_decode_lu32(const uint8_t chave, uint32_t valor[], const uint16_t cont_list_a);
int32_t gilson_decode_ls32(const uint8_t chave, int32_t valor[], const uint16_t cont_list_a);
int32_t gilson_decode_lu64(const uint8_t chave, uint64_t valor[], const uint16_t cont_list_a);
int32_t gilson_decode_ls64(const uint8_t chave, int64_t valor[], const uint16_t cont_list_a);
int32_t gilson_decode_lf32(const uint8_t chave, float valor[], const uint16_t cont_list_a);
int32_t gilson_decode_lf64(const uint8_t chave, double valor[], const uint16_t cont_list_a);
int32_t gilson_decode_lstr(const uint8_t chave, char *valor, const uint16_t cont_list_a, const uint16_t cont_list_b);

//========================================= GSON_MTX2D
int32_t gilson_decode_mu8(const uint8_t chave, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_decode_ms8(const uint8_t chave, int8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_decode_mu16(const uint8_t chave, uint16_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_decode_ms16(const uint8_t chave, int16_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_decode_mu32(const uint8_t chave, uint32_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_decode_ms32(const uint8_t chave, int32_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_decode_mu64(const uint8_t chave, uint64_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_decode_ms64(const uint8_t chave, int64_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_decode_mf32(const uint8_t chave, float *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_decode_mf64(const uint8_t chave, double *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);

#if (TESTE_GSON_MODO_JSON==1)
// experimental... provável que não vale a pena
int32_t gilson_encode_from_json(const char *json_in, uint8_t *pack, const uint32_t size_max_pack);
int32_t gilson_decode_to_json(uint8_t *pack, char *json_out);
#endif  // #if (TESTE_GSON_MODO_JSON==1)

#endif /* GILSON_H_ */
