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
 Version		: 0.58
 Date			: 17/02/26
 Description	: biblioteca 'gilson'
 GitHub			: https://github.com/casimirdes/gilson
 ============================================================================
 */



#ifndef GILSON_H_
#define GILSON_H_

// para fins de plataforma e debug
#define GIL_TYPE_DEVICE			1  // 0=microcontrolador, 1=PC
#define GIL_DEBUG_LIB			0  // ativa os prints debug, mais dedicado a PC
#define GIL_PRINT_DEBUG			0  // 1 = printa toda vida o debug

#define GIL_TESTE_MODO_JSON 	0  // ainda experimental...
#define GIL_FLAG_NEW_KEY		1  // para erros do tipo 'erGIL_23b' e 'erGIL_30'::: 0=retorno com erro, 1=avança e limpa data

#define GIL_LIMIT_KEYS			255  // até 255 chaves cada pacote gilson, (0 a 254)
#define GIL_LIMIT_STRING		255  // tamanho maximo de um item que é do tipo string, seja 'GIL_SINGLE' ou 'GIL_LIST'
#define GIL_LIMIT_KEY_NAME		32  // tamanho máximo do nome chave de cada elemento ala JSON quando utilizado modo 'GIL_MODO_KV'

// GIL_SIZE_RAM = como a RAM é 'medida/vista' no sistema: uint8_t, uint16_t, uint32_t, uint64_t, (ex.: PC de 64bits = uint64_t, STM32/ESP32 = 32bits = uint32_t, atmega328p=uint16_t)
#if (GIL_TYPE_DEVICE==1)
#define GIL_SIZE_RAM		uint64_t
#else
#define GIL_SIZE_RAM		uint32_t
#endif  // #if (GIL_TYPE_DEVICE==1)

#define CAST_GIL	(uint8_t *)&  // forma bem deselegante de padronizar para fazer o 'casting'


enum e_MODO_GILSON
{
	GIL_MODO_ZIP,  	// é cru, sem nada, somente a data bruta, tem 1 byte adicional que é [0]=modo
	GIL_MODO_FULL,  	// modo padão com offset, crc, identificador
	GIL_MODO_KV,  		// modo GIL_MODO_FULL + KV ala key:value chave:valor salva nome em string das chaves ala JSON da vida
	GIL_MODO_KV_ZIP,  	// modo GIL_MODO_ZIP porem salva os nomes das chaves ala JSON da vida
	GIL_MODO_JSON,		// caso particular de encode/decode de um JSON estático *** só usa com funcoes "xxxxx_json" experimental...
	GIL_MODO_MAX
};

enum e_TIPOS1_GILSON
{
	GIL_SINGLE,  		// valor unico
	GIL_LIST,  		// é no formato lista, [u16] mas até 64k
	GIL_MTX2D,  		// é no formato matriz, max 2 dimenções!!! [u8][u16] mas jamais pode passar de 64k!!!!
	GIL_MAX
	// GIL_MAX == 0b111, 0 a 7
};

enum e_TIPOS2_GILSON
{
	// reserva o '0' para outros usos...
	GIL_tBIT=1,
	GIL_tINT8,
	GIL_tUINT8,
	GIL_tINT16,
	GIL_tUINT16,
	GIL_tINT32,
	GIL_tUINT32,
	GIL_tINT64,
	GIL_tUINT64,
	GIL_tFLOAT32,
	GIL_tFLOAT64,
	GIL_tSTRING,
	GIL_tMAX,
	// GIL_tMAX == 0b1110 (1 a 30)
};


enum e_er_GILSON
{
	// vai fazer 'erGIL_xxxx' onde xxxx-1000 = erro retornado
	erGIL_OK = 0,
	erGIL_NULL = -1000,  	// add chave 'null' no modo ZIP
	erGIL_OPER,				// erro de operação, está em enconde e quer usar funcao de decode ou contrário
	erGIL_DIFKEY,  			// validando decode, total de chaves mapa diferentes com pacote
	erGIL_DIFKEYb,  		// validar pacote que não é FULL
	erGIL_eJSON,  			// está querendo encodar 'GIL_MODO_JSON' no encode errado
	erGIL_dJSON,  			// está querendo decoder 'GIL_MODO_JSON' no decode errado
	erGIL_LIMKEY,  			// limite de chaves, atual = 256 (0 a 255)
	erGIL_SMKEYe,  			// chave ja foi adicionada no encode
	erGIL_SMKEYd,  			// chave ja foi adicionada no decode
	erGIL_0,  				// gilson_encode_init: já tem um pacote ativo
	erGIL_1,  				// gilson_encode_init: 2 pacotes ativos, não é possível iniciar
	erGIL_2,  				// gilson_encode_init: quer utililzar o mesmo buffer geral na RAM????? não tem fazer isso...
	erGIL_3,  				// gilson_encode_data_base: não chamou init encode
	erGIL_4,  				// gilson_encode_data_base: erro tipo1
	erGIL_4b,  				// gilson_encode_data_base: erro tipo2
	erGIL_5,  				// gilson_encode_data_base: quer add uma chave maior do que a contagem crescente... não tem como
	erGIL_6,  				// explodiu o buffer de entrada
	erGIL_7,  				// no modo KV, tamanho do nome da chave é maior que GIL_LIMIT_KEY_NAME
	erGIL_8,  				// tamanho da lista está zerado cont_list_a==0
	erGIL_9,  				// lista de string sem definir tamanho max da string
	erGIL_9b,  				// tamanho da string passa do limite permitido (>= LEN_MAX_STRING)
	erGIL_10,  				// matriz de string não é permitido
	erGIL_11,  				// matriz com algum cont_list_X zerado
	erGIL_12,  				// lista de string sem definir tamanho max da string
	erGIL_12b,  			// tamanho da string passa do limite permitido (>= LEN_MAX_STRING)
	erGIL_13,  				// gilson_encode_data_base: tipo2 inválido
	erGIL_14,  				// string ficou maior que LEN_MAX_STRING
	erGIL_15,  				// gilson_decode_init: quer decodificar e já está com pacotes ativos
	erGIL_16,  				// gilson_decode_init: quer decodificar e já está com os 2 pacotes ativos
	erGIL_17,  				// gilson_decode_init: quer utililzar o mesmo buffer geral????? não tem fazer isso....
	erGIL_18,  				// gilson_decode_init: no decode, pacote com erro de 'GIL_MODO_MAX'
	erGIL_19,  				// gilson_decode_init: erro valida crc pacote FULL
	erGIL_20,  				// gilson_decode_end_base: chegou até o fim 'cont_itens2' mas o 'pos_bytes' não bateu com o 'pos_bytes2', dai deu ruim
	erGIL_21,  				// gilson_decode_data_base: pacote não está ativo
	erGIL_22,  				// gilson_decode_data_base: tipo1 inválido
	erGIL_22b,  			// gilson_decode_data_base: tipo2 inválido
	erGIL_23,  				// gilson_decode_data_base: quer ler chave maior fora da ordem crescente, ZIP/FULL
	erGIL_23b,  			// gilson_decode_data_base: quer ler chave que não existe, maior que programado, ZIP/FULL
	erGIL_24,  				// gilson_decode_data_base: lista com 'cont_list_a' zerado
	erGIL_25,  				// gilson_decode_data_base: lista de strings com 'cont_list_b' zerado
	erGIL_26,  				// gilson_decode_data_base: matriz com algum 'cont_list_X' zerado
	erGIL_DIFIN,  			// gilson_decode_data_base: no decode, diferenças de paramentros de entrada no modo FULL
	erGIL_27,  				// gilson_decode_data_base: tipo2 inválido
	erGIL_28,  				// gilson_decode_data_full_base: pacote não está ativo
	erGIL_29,  				// gilson_decode_data_full_base: modo incompatível, tem que ser FULL
	erGIL_30,  				// gilson_decode_data_full_base: quer ler uma chave que não existe, modo FULL, tipo 'erGIL_23b'
	erGIL_31,  				// gilson_decode_data_full_base: tipo1 inválido
	erGIL_31b,  			// gilson_decode_data_full_base: tipo2 inválido
	erGIL_32,  				// gilson_decode_data_full_base: lista com 'cont_list_a' zerado
	erGIL_33,  				// gilson_decode_data_full_base: lista de strings com 'cont_list_b' zerado
	erGIL_34,  				// gilson_decode_data_full_base: matriz com algum 'cont_list_X' zerado
	erGIL_35,  				// gilson_decode_data_full_base: tipo2 inválido
	erGIL_36,  				// gilson_encode_dl_init: pacote não está ativo
	erGIL_37,  				// gilson_encode_dl_init: ja está ativo esse modo... tem que teminar antes para iniciar um novo
	erGIL_38,  				// gilson_encode_dl_init: quer add uma chave maior do que a contagem crescente... não tem como
	erGIL_39,  				// valid_gilson_encode_dl: pacote não está ativo
	erGIL_40,  				// valid_gilson_encode_dl: tipo não é dinâmico
	erGIL_41,  				// valid_gilson_encode_dl: tipo1 inválido
	erGIL_41b,  			// valid_gilson_encode_dl: tipo2 inválido
	erGIL_42,  				// valid_gilson_encode_dl: quer add uma item maior do que a contagem crescente
	erGIL_43,  				// valid_gilson_encode_dl: lista com 'cont_list_a' zerado
	erGIL_44,  				// valid_gilson_encode_dl: lista de string com 'cont_list_b' zerado
	erGIL_45,  				// valid_gilson_encode_dl: matriz de string não pode
	erGIL_46,  				// valid_gilson_encode_dl: matriz com algum 'cont_list_X' zerado
	erGIL_47,  				// valid_gilson_encode_dl: lista de string sem definir tamanho max da string
	erGIL_48,  				// gilson_encode_dl_end: pacote não é tipo dinâmico
	erGIL_49,  				// gilson_encode_dl_end: total de itens geral não bate a conta com o programado
	erGIL_50,  				// gilson_decode_dl_init: pacote não está ativo
	erGIL_51,  				// gilson_decode_dl_init: ja está ativo esse modo... tem que teminar antes para iniciar um novo
	erGIL_52,  				// gilson_decode_dl_init: quer ler uma chave maior do que a contagem programada
	erGIL_53,  				// gilson_decode_dl_init: identificador 'TIPO_GIL_LDIN' inválido
	erGIL_54,  				// gilson_decode_dl_init: tipo1 inválido
	erGIL_54b,  			// gilson_decode_dl_init: tipo2 inválido
	erGIL_55,  				// gilson_decode_dl_init: lista com 'cont_list_a' zerado
	erGIL_56,  				// gilson_decode_dl_init: lista de string com 'cont_list_b' zerado
	erGIL_57,  				// gilson_decode_dl_init: matriz com algum 'cont_list_X' zerado
	erGIL_58,  				// gilson_decode_dl_data: pacote não é modo FULL
	erGIL_59,  				// gilson_decode_dl_data: quer ler uma item maior do que a contagem crescente... não tem como
	erGIL_60,  				// gilson_decode_dl_end: pacote não é tipo dinâmico
	erGIL_61,  				// gilson_decode_dl_end: total de itens geral não bate a conta com o programado
	erGIL_62,  				// gilson_decode_key: pacote não é modo FULL
	erGIL_63,  				// gilson_encode_dl_init: pacote não é modo FULL
	erGIL_64,  				//
	erGIL_65,  				//
	erGIL_66,  				//
	erGIL_67,  				//
	erGIL_68,  				//
	erGIL_69,  				//
	erGIL_70,  				//
	erGIL_71,  				//
	erGIL_72,  				//
	erGIL_73,  				//
	erGIL_74,  				//
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

int32_t gilson_decode_valid_map(const uint16_t map_full[][6], uint16_t tot_chaves, const uint8_t *pack);

int32_t gilson_decode_key(const uint8_t *pack, const uint8_t chave, uint8_t *valor);

// decode lista dinâmica "dl"
int32_t gilson_decode_dl_init(const uint8_t chave);
//int32_t gilson_decode_dl_data_zip(const uint8_t item, const uint8_t tipo1, const uint8_t tipo2, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_decode_dl_data(const uint8_t item, uint8_t *valor);
int32_t gilson_decode_dl_end(void);


// funcoes auxiliares fortemente tipadas de encode
//========================================= GIL_SINGLE
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

//========================================= GIL_LIST
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

//========================================= GIL_MTX2D
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
//========================================= GIL_SINGLE
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

//========================================= GIL_LIST
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

//========================================= GIL_MTX2D
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

#if (GIL_TESTE_MODO_JSON==1)
// experimental... provável que não vale a pena
int32_t gilson_encode_from_json(const char *json_in, uint8_t *pack, const uint32_t size_max_pack);
int32_t gilson_decode_to_json(uint8_t *pack, char *json_out);
#endif  // #if (GIL_TESTE_MODO_JSON==1)

#endif /* GILSON_H_ */
