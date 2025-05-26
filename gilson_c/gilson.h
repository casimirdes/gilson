
/*
 ============================================================================
 Name			: gilson.h
 Author			: mjm
 Version		: 0.5
 Date			: 24/05/25
 Description : biblioteca 'gilson'
 ============================================================================
 */



#ifndef GILSON_H_
#define GILSON_H_


#define CAST_GIL	(uint8_t *)&  // fica estranho mas vamos ver como funciona, senao removo na proxima

enum e_MODO_GILSON
{
	GSON_MODO_ZIP,  	// é cru, sem nada, somente a data bruta, tem 1 byte adicional que é [0]=modo
	GSON_MODO_FULL,  	// modo padão com offset, crc, identificador
	GSON_MODO_KV,  		// modo GSON_MODO_FULL + KV ala key:value chave:valor salva nome em string das chaves ala JSON da vida
	GSON_MODO_KV_ZIP,  	// modo GSON_MODO_ZIP porem salva os nomes das chaves ala JSON da vida
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
	// GSON_tMAX == 0b1111 (1 a 31)
};


enum e_erros_GILSON
{
	// vai fazer 'erGSON_xxxx' onde xxxx-1000 = erro retornado
	erGSON_OK = 0,
	erGSON_0 = -1000,
	erGSON_1,
	erGSON_2,
	erGSON_3,
	erGSON_4,
	erGSON_5,
	erGSON_6,
	erGSON_7,
	erGSON_8,
	erGSON_9,
	erGSON_10,
	erGSON_11,
	erGSON_12,
	erGSON_13,
	erGSON_14,
	erGSON_15,
	erGSON_16,
	erGSON_17,
	erGSON_18,
	erGSON_19,
	erGSON_20,
	erGSON_21,
	erGSON_22,
	erGSON_23,
	erGSON_24,
	erGSON_25,
	erGSON_26,
	erGSON_27,
	erGSON_28,
	erGSON_29,
	erGSON_30,
	erGSON_31,
	erGSON_32,
	erGSON_33,
	erGSON_34,
	erGSON_35,
	erGSON_36,
	erGSON_37,
	erGSON_38,
	erGSON_39,
	erGSON_40,
	erGSON_41,
	erGSON_42,
	erGSON_43,
	erGSON_44,
	erGSON_45,
	erGSON_46,
	erGSON_47,
	erGSON_48,
	erGSON_49,
	erGSON_50,
	erGSON_51,
	erGSON_52,
	erGSON_53,
	erGSON_54,
	erGSON_55,
	erGSON_56,
	erGSON_57,
	erGSON_58,
	erGSON_59,
	erGSON_60,
	erGSON_61,
	erGSON_62,
	erGSON_63,
	erGSON_64,
	erGSON_65,
	erGSON_66,
};


// parte de encode ======================================================
int32_t gilson_encode_init(const uint8_t modo_, uint8_t *pack, const uint16_t size_max_pack);
int32_t gilson_encode_data(const uint8_t chave, const uint8_t tipo1, const uint8_t tipo2, const uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_encode_dataKV(const uint8_t chave, const uint8_t tipo1, const uint8_t tipo2, char *nome_chave, const uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_encode(const uint8_t chave, const uint8_t tipo1, const uint8_t tipo2, ...);
int32_t gilson_encode_mapfix(const uint16_t *map, ...);
int32_t gilson_encode_mapdin(const uint16_t *map, ...);
int32_t gilson_encode_end(uint32_t *crc);

// encode lista dinâmica "dl"
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
int32_t gilson_decode_end(uint32_t *crc);

// decode lista dinâmica "dl"
int32_t gilson_decode_dl_init(const uint8_t chave);
int32_t gilson_decode_dl_data(const uint8_t item, uint8_t *valor);
int32_t gilson_decode_dl_end(void);

#endif /* GILSON_H_ */
