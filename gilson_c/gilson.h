
/*
 ============================================================================
 Name        : gilson.h
 Author      : mjm
 Version     : 0.2
 Date		 : 30/12/24
 Description : biblioteca 'gilson'
 ============================================================================
 */



#ifndef GILSON_H_
#define GILSON_H_

#define CAST_GIL	(uint8_t *)&

enum e_MODO_GILSON
{
	GSON_MODO_ZIP,  // é cru, sem nada, somente a data bruta, tem 1 byte adicional que é [0]=modo
	GSON_MODO_FULL,  // modo padão com offset, crc, identificador
	GSON_MODO_KV,  // modo GSON_MODO_FULL + KV ala key:value chave:valor salva nome em string das chaves ala JSON da vida
	GSON_MODO_KV_ZIP,  // modo GSON_MODO_ZIP porem salva os nomes das chaves ala JSON da vida
	GSON_MODO_MAX
};

enum e_TIPOS1_GILSON
{
	GSON_SINGLE,  // valor unico
	GSON_LIST,  // é no formato lista, [u16] mas até 64k
	GSON_MTX2D,  // é no formato matriz, max 2 dimenções!!! [u8][u16] mas jamais pode passar de 64k!!!!
	GSON_MAX
	// GSON_MAX == 0b111, 0 a 7
};

enum e_TIPOS2_GILSON
{
	GSON_tBIT,
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
	// GSON_tMAX == 0xf, 0 a 15 ou futuramente 0 a 31
};



int32_t gilson_encode_init(const uint8_t modo_, uint8_t *pack, const uint16_t size_max_pack);
int32_t gilson_encode_data(const uint8_t chave, const char *nome_chave, const uint8_t tipo1, const uint8_t tipo2, const uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_encode_end(uint32_t *crc);

int32_t gilson_decode_init(const uint8_t *pack);
int32_t gilson_decode_valid(const uint8_t *pack);
int32_t gilson_decode_data(const uint8_t chave, char *nome_chave, const uint8_t tipo1, const uint8_t tipo2, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step);
int32_t gilson_decode_data_full(const uint8_t chave, char *nome_chave, uint8_t *valor);
int32_t gilson_decode_end(uint32_t *crc);


#endif /* GILSON_H_ */
