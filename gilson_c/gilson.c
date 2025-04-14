
/*
 ============================================================================
 Name			: gilson.c
 Author			: mjm
 Version		: 0.31
 Date			: 12/04/25
 Description : biblioteca 'gilson'
 ============================================================================
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>


#include "gilson.h"

#define USO_DEBUG_LIB		0  // 0=microcontrolador, 1=PC

#define SIZE_TYPE_RAM	uint64_t  // uint8_t, uint16_t, uint32_t, uint64_t

#define LEN_PACKS_GILSON	2  // 2 pacotes manipulaveis "ao mesmo tempo", se abriu o segundo tem que fechar para voltar para o primeiro!!!


#define OFFSET_MODO_ZIP		1
#define OFFSET_MODO_FULL	8

#define LEN_MAX_CHAVE_NOME	16  // tamanho máximo do nome chave de cada elemento ala JSON quando utilizado modo 'GSON_MODO_KV'

// estrutura de controle da lib gilson
typedef struct {
	SIZE_TYPE_RAM end_ram;
	uint32_t crc;
	uint32_t crc_out;  // quando decodifica
	int32_t erro;  // backup de erro geral de cada operacao...
	uint16_t pos_bytes, pos_bytes2, pos_bytes_bk, size_max_pack;
	uint8_t modo;  // 0=padrao, 1=compacto
	uint8_t ativo;
	uint8_t cont_itens, cont_itens2, cont_itens_old;
	uint8_t chave_atual;  // para fins de comparar e validar chaves durante encode
	uint8_t *bufw;  // buffer de escrita/leitura
	const uint8_t *bufr;  // buffer somente leitura
}struct_gilson;


static struct_gilson s_gil[LEN_PACKS_GILSON]={0};
static uint8_t ig=0;


// copiado da lib do littlefs "lfs_crc()"
// Software CRC implementation with small lookup table
static uint32_t fs_crc(uint32_t crc, const uint8_t *buffer, const uint16_t size)
{
    static const uint32_t rtable[16] = {
        0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
        0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
        0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
        0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
    };

    const uint8_t *data = buffer;

    for (size_t i = 0; i < size; i++)
    {
        crc = (crc >> 4) ^ rtable[(crc ^ (data[i] >> 0)) & 0xf];
        crc = (crc >> 4) ^ rtable[(crc ^ (data[i] >> 4)) & 0xf];
    }

    return crc;
}


//========================================================================================================================
//========================================================================================================================
//========================================================================================================================
//========================================================================================================================
// PARTE DE CODIFICAÇÃO

int32_t gilson_encode_init(const uint8_t modo_, uint8_t *pack, const uint16_t size_max_pack)
{
	SIZE_TYPE_RAM end_ram=0;
	int32_t erro=erGSON_OK;

	// queremos codificar um pacote...

	if(s_gil[ig].ativo==1)
	{
		erro = erGSON_0;
		end_ram = (SIZE_TYPE_RAM)pack;  // endereço na ram????

		// atual 'ig' está ativo ja... nao tem o que fazer... vamos consultar o segundo
		if(ig==0)
		{
			// vamos ver o segundo...
			if(s_gil[ig+1].ativo==1)
			{
				erro = erGSON_1;
				// nao tem o que fazer.... aborta a missao!!!!
			}
			else if(end_ram==s_gil[ig].end_ram)
			{
				// quer utililzar o mesmo vetor geral????? nao tem fazer isso....
				erro = erGSON_2;
				// nao tem o que fazer.... aborta a missao!!!!
			}
			else
			{
				erro = erGSON_OK;
				ig=1;  // vai para o segundo e esse passar ser o atual global da lib
			}
		}

		if(erro!=erGSON_OK)
		{
			goto deu_erro;
		}
	}

	memset(&s_gil[ig], 0x00, sizeof(s_gil[ig]));

	s_gil[ig].modo = modo_;
	s_gil[ig].bufw = pack;
	s_gil[ig].end_ram = (SIZE_TYPE_RAM)pack;  // endereço na ram????
	s_gil[ig].size_max_pack = size_max_pack;
	//memset(s_gil[ig].bufw, 0x00, sizeof(*s_gil[ig].bufw));

	s_gil[ig].cont_itens=0;

	if(s_gil[ig].modo == GSON_MODO_ZIP || s_gil[ig].modo == GSON_MODO_KV_ZIP)
	{
		s_gil[ig].pos_bytes = OFFSET_MODO_ZIP;  // offset
	}
	else
	{
		s_gil[ig].pos_bytes = OFFSET_MODO_FULL;  // offset
	}

	deu_erro:

	s_gil[ig].erro = erro;

#if (USO_DEBUG_LIB==1)
	printf("DEBUG gilson_encode_init::: ig:%u, erro:%i, modo:%u, pos_bytes:%u, cont_itens:%u, end_ram:%u|%u\n", ig, erro, s_gil[ig].modo, s_gil[ig].pos_bytes, s_gil[ig].cont_itens, s_gil[ig].end_ram, end_ram);
#endif

	s_gil[ig].ativo = 1;  // ativa a estrutura 'ig' da vez!!!
	return erro;
}


int32_t gilson_encode_end(uint32_t *crc)
{
	s_gil[ig].crc=0;

	// 8 bytes de offset geral:
	// [0] 1b = modo
	// [1:4] 4b = crc pacote (crc32)
	// [5:6] 2b = tamanho pacote (até 65355 bytes)
	// [7] 1b = quantos elementos geral (nao soma as listas, trata como 1) até 255 elementos/itens
	// [8::] data

	s_gil[ig].bufw[0] = s_gil[ig].modo;

	if(s_gil[ig].modo == GSON_MODO_ZIP || s_gil[ig].modo == GSON_MODO_KV_ZIP)
	{
		s_gil[ig].crc = fs_crc(0xffffffff, s_gil[ig].bufw, s_gil[ig].pos_bytes);  // chamar depois que 'pos_bytes' está completo!!!!
	}
	else  // outros modos
	{
		memcpy(&s_gil[ig].bufw[5], &s_gil[ig].pos_bytes, 2);
		s_gil[ig].bufw[7] = s_gil[ig].cont_itens;

		s_gil[ig].crc = fs_crc(0xffffffff, &s_gil[ig].bufw[5], s_gil[ig].pos_bytes-5);

		memcpy(&s_gil[ig].bufw[1], &s_gil[ig].crc, 4);

	}

#if (USO_DEBUG_LIB==1)
	printf("DEBUG gilson_encode_end::: ig:%u, modo:%u pos_bytes:%u cont_itens:%u crc:%u cru:[%u,%u,%u,%u,%u,%u,%u,%u]\n", ig, s_gil[ig].modo, s_gil[ig].pos_bytes, s_gil[ig].cont_itens, s_gil[ig].crc, s_gil[ig].bufw[0], s_gil[ig].bufw[1], s_gil[ig].bufw[2], s_gil[ig].bufw[3], s_gil[ig].bufw[4], s_gil[ig].bufw[5], s_gil[ig].bufw[6], s_gil[ig].bufw[7]);
#endif

	s_gil[ig].ativo = 0;

	*crc = s_gil[ig].crc;

	if(ig==1)
	{
		ig = 0;  // volta para o primeiro, pois está aberto!!!
		return s_gil[ig+1].pos_bytes;
	}
	else
	{
		return s_gil[ig].pos_bytes;
	}
}



// colocar sempre o "(uint8_t *)" na frente da variavel de entrada 'multi_data' e passar como "&"
int32_t gilson_encode_data_base(const uint8_t chave, const char *nome_chave, const uint8_t tipo1, const uint8_t tipo2, const uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	int32_t erro=erGSON_OK, len=0;  // check
	uint16_t i, vezes = 1, nbytes = 1, temp16, pos_bytes_bk=0;
	uint8_t len_string_max=0, tipo_mux=0, flag_teste=0;
	char buff[256];  // supondo tamanho maximo de strings

	if(s_gil[ig].erro != erGSON_OK)
	{
		erro = s_gil[ig].erro;  // vamos manter sempre o mesmo erro!!!
		goto deu_erro;
	}

	if(s_gil[ig].ativo==0)
	{
		erro = erGSON_3;
		goto deu_erro;
	}

	if(tipo1>=GSON_MAX || tipo2>=GSON_tMAX)
	{
		erro = erGSON_4;
		goto deu_erro;
	}

	if(chave > s_gil[ig].cont_itens)
	{
		// quer add uma chave maior do que a contagem crescente... nao tem como
		erro = erGSON_5;
		goto deu_erro;
	}

	/*
	//--------------------------------------------------------------------
	// varifica quantos bytes vai precisar para essa chave...
	check = gilson_encode_pack_chave_len(nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step);
	if(check<0 || ((s_gil[ig].pos_bytes + check) > s_gil[ig].size_max_pack))
	{
		// deu erro antes de saber ou vai explodir o buffer de entrada!!!
		erro = erGSON_6;
		goto deu_erro;
	}
	//---------------------------------------------------------------------
	*/
	flag_teste = 1;  // parte em 1 pois vai fazer o teste primeiro... depois retorna e mete bala na real da vida
	pos_bytes_bk = s_gil[ig].pos_bytes;  // faz o backup atual
	goto_meta_bala:


	if(s_gil[ig].modo == GSON_MODO_KV || s_gil[ig].modo == GSON_MODO_KV_ZIP)
	{
		len = strlen(nome_chave);
		if(len>LEN_MAX_CHAVE_NOME)
		{
			erro = erGSON_7;
			goto deu_erro;
		}
		else
		{
			if(flag_teste==0)
			{
				s_gil[ig].bufw[s_gil[ig].pos_bytes] = (uint8_t)len;
			}
			s_gil[ig].pos_bytes += 1;

			if(flag_teste==0)
			{
				memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], nome_chave, len);
			}
			s_gil[ig].pos_bytes += len;
		}
	}

	// 0baaabbbbb = a:tipo1, b=tipo2
	tipo_mux = tipo1<<5;
	tipo_mux |= tipo2;
	//printf("encode: 0baaabbbbb = a:tipo1(%u), b=tipo2()%u tipo_mux:%u\n", tipo1, tipo2, tipo_mux);

	if(s_gil[ig].modo == GSON_MODO_FULL || s_gil[ig].modo == GSON_MODO_KV)
	{
		if(flag_teste==0)
		{
			s_gil[ig].bufw[s_gil[ig].pos_bytes] = tipo_mux;
		}
		s_gil[ig].pos_bytes += 1;
	}

	if(tipo1 == GSON_LIST)
	{
		if(cont_list_a == 0)
		{
			erro = erGSON_8;
			goto deu_erro;
		}
		vezes = cont_list_a;

		if(s_gil[ig].modo == GSON_MODO_FULL || s_gil[ig].modo == GSON_MODO_KV)
		{
			if(flag_teste==0)
			{
				memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], &vezes, 2);
			}
			s_gil[ig].pos_bytes += 2;
			//printf("DEBUG gilson_encode_data: [%u,%u,%u,%u]\n", bufw[pos_bytes-4], bufw[pos_bytes-3], bufw[pos_bytes-2], bufw[pos_bytes-1]);
		}

		if(tipo2==GSON_tSTRING)
		{
			// aqui 'cont_list_b' é tratado como uint8

			if(cont_list_b==0)
			{
				// precisamos do offset cru da lista de strings...
				erro = erGSON_9;
				goto deu_erro;
			}
			else
			{
				len_string_max = cont_list_b;
				if(s_gil[ig].modo == GSON_MODO_FULL || s_gil[ig].modo == GSON_MODO_KV)
				{
					if(flag_teste==0)
					{
						s_gil[ig].bufw[s_gil[ig].pos_bytes] = len_string_max;
					}
					s_gil[ig].pos_bytes += 1;
				}
			}
		}
	}
	else if(tipo1 == GSON_MTX2D)
	{
		// aqui 'cont_list_b' é tratado como uint16
		if(tipo2==GSON_tSTRING)
		{
			// nao testado isso ainda, vai da ruim
			erro = erGSON_10;
			goto deu_erro;
		}

		if(cont_list_a == 0 || cont_list_b == 0 || cont_list_step == 0)
		{
			erro = erGSON_11;
			goto deu_erro;
		}
		//vezes = cont_list_a * cont_list_b;  nao utiliza...

		if(s_gil[ig].modo == GSON_MODO_FULL || s_gil[ig].modo == GSON_MODO_KV)
		{
			if(flag_teste==0)
			{
				s_gil[ig].bufw[s_gil[ig].pos_bytes] = (uint8_t)cont_list_a;
			}
			s_gil[ig].pos_bytes += 1;
			if(flag_teste==0)
			{
				memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], &cont_list_b, 2);
			}
			s_gil[ig].pos_bytes += 2;
			if(flag_teste==0)
			{
				memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], &cont_list_step, 2);
			}
			s_gil[ig].pos_bytes += 2;
			//printf("DEBUG gilson_encode_data: [%u,%u,%u,%u]\n", bufw[pos_bytes-4], bufw[pos_bytes-3], bufw[pos_bytes-2], bufw[pos_bytes-1]);
		}
	}
	else  // GSON_SINGLE
	{
		vezes = 1;

		if(tipo2==GSON_tSTRING)
		{
			if(cont_list_a==0)
			{
				erro = erGSON_12;
				goto deu_erro;
			}
			else
			{
				len_string_max = cont_list_a;
			}
		}
	}


	switch(tipo2)
	{
	case GSON_tBIT:
		// nada ainda...
		break;
	case GSON_tINT8:
	case GSON_tUINT8:
		nbytes = 1;
		break;
	case GSON_tINT16:
	case GSON_tUINT16:
		nbytes = 2;
		break;
	case GSON_tINT32:
	case GSON_tUINT32:
		nbytes = 4;
		break;
	case GSON_tINT64:
	case GSON_tUINT64:
		nbytes = 8;
		break;
	case GSON_tFLOAT32:
		nbytes = 4;
		break;
	case GSON_tFLOAT64:
		nbytes = 8;
		break;
	case GSON_tSTRING:
		// só vai...
		break;
	default:
		erro = erGSON_13;
		goto deu_erro;
	}


	if(tipo2 == GSON_tSTRING)
	{
		for(i=0; i<vezes; i++)
		{
			// OBS: é obrigado ter o 'len_string_max' que é o valor de 'cont_list_b'!!!!!!!!!!!!
			memset(buff, 0x00, sizeof(buff));  // limpeza pra evitar lixos antigos
			memcpy(buff, valor+(i*len_string_max), len_string_max);
			len = strlen(buff);  // vai esbarrar no primero '\0' que encontrar...
			// problema é com a codificação utf-8 que se formos truncar tem que cuidar onde truncar...

			//printf("DEBUGGGGGGGGGGGGGGGGGGGGGG len:%u, len_string_max:%u, |%s| |%s|\n", len, len_string_max, multi_data, buff);

			if(len>=len_string_max)
			{
				// nao temos '\0'???? caso len==len_string_max, string ta no limite!!!

				/*
				len = len_string_max;
				buff[len-1] = 0;  // para garantir o '\0'
				if(buff[len-2]>0 && (buff[len-2]<32 || buff[len-2]>126))  // tabela ascii, se caso ficou bugado em um utf-8
				{
					buff[len-2] = 0;
				}
				len = strlen(buff);  // atualiza os ajustes
				*/

				// vamos ver se temos um caso de utf-8 no ultimo byte...
				if(buff[len-1]>0 && (buff[len-1]<32 || buff[len-1]>126))  // tabela ascii, se caso ficou bugado em um utf-8
				{
					buff[len-1] = 0;
					if(buff[len-2]>0 && (buff[len-2]<32 || buff[len-2]>126))  // tabela ascii, se caso ficou bugado em um utf-8
					{
						buff[len-2] = 0;
					}
					len = strlen(buff);  // atualiza os ajustes
				}
			}

			if(len<=255)
			{
				if(flag_teste==0)
				{
					s_gil[ig].bufw[s_gil[ig].pos_bytes] = (uint8_t)len;
				}
				s_gil[ig].pos_bytes += 1;

				if(flag_teste==0)
				{
					memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], buff, len);
				}
				s_gil[ig].pos_bytes += len;
			}
			else
			{
				erro = erGSON_14;  // nao é pra cair aqui pois o max de 'len_string_max' é 255 u8
				break;
			}
		}
	}
	else if(tipo2 == GSON_tBIT)
	{
		// nada ainda...
	}
	else  // para o resto é só varer bytes
	{
		if(tipo1 == GSON_MTX2D)
		{
			// vamos ao exemplo:
			// float x[10][250] e na realidade vamos usar somente x[2][3] ==== x[cont_list_a][cont_list_b] com step de 'cont_list_step'
			// 10*250=2500 elementos serializados e como é float *4 = 10000 bytes, mas a serializacao vai ir [0][0], [0][1]...[0][249], [1][0] ... [9][249]
			// logo se eu quero x[2][3] tenho que dar os offset com base nos valores máximos (250 elementos cada linha)
			for(i=0; i<cont_list_a; i++)
			{
				temp16 = i*cont_list_step*nbytes;  // passos em cada linha da matriz
				if(flag_teste==0)
				{
					memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], valor+temp16, nbytes*cont_list_b);
				}
				s_gil[ig].pos_bytes += nbytes*cont_list_b;
			}
		}
		else
		{
			if(flag_teste==0)
			{
				memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], valor, nbytes*vezes);
			}
			s_gil[ig].pos_bytes += nbytes*vezes;
		}
	}

	/*
	if(flag_teste)
	{
		flag_teste = 0;  // desativa modo teste!!!
		s_gil[ig].pos_bytes = pos_bytes_bk;  // restaura backup atual
		goto goto_meta_bala;
	}
	*/

	deu_erro:

	if(flag_teste)
	{
		if(erro!=erGSON_OK || (s_gil[ig].pos_bytes > s_gil[ig].size_max_pack))
		{
			// deu erro antes de saber ou vai explodir o buffer de entrada!!!
			if(erro==erGSON_OK)
			{
				// específico que explodiu o buffer de entrada
				erro = erGSON_6;
			}
		}
		else
		{
			flag_teste = 0;  // desativa modo teste!!!
			s_gil[ig].pos_bytes = pos_bytes_bk;  // restaura backup atual
			goto goto_meta_bala;
		}
	}

	if(erro==erGSON_OK)
	{
		s_gil[ig].cont_itens += 1;
	}
	else
	{
		s_gil[ig].erro = erro;
#if (USO_DEBUG_LIB==1)
		printf("DEBUG gilson_encode_data::: ig:%u, ERRO:%i, modo:%u, tipo1:%u, tipo2:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u, flag_teste:%u\n", ig, erro, s_gil[ig].modo, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step, flag_teste);
#endif
	}

	return erro;
}


int32_t gilson_encode_data(const uint8_t chave, const uint8_t tipo1, const uint8_t tipo2, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	return gilson_encode_data_base(chave, "x", tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step);
}

int32_t gilson_encode_dataKV(const uint8_t chave, const uint8_t tipo1, const uint8_t tipo2, char *nome_chave, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	return gilson_encode_data_base(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step);
}


int32_t gilson_encode(const uint8_t chave, const uint8_t tipo1, const uint8_t tipo2, ...)
{
	uint16_t cont_list_a=0, cont_list_b=0, cont_list_step=0;
	uint8_t *valor;
	char *nome_chave = {"\0"};
	va_list argptr;

	va_start(argptr, tipo2);

	if(s_gil[ig].modo == GSON_MODO_KV || s_gil[ig].modo == GSON_MODO_KV_ZIP)
	{
		nome_chave = va_arg(argptr, char *);
	}

	valor = va_arg(argptr, uint8_t *);

	if(tipo1 == GSON_SINGLE)
	{
		if(tipo2 == GSON_tSTRING)
		{
			// erro = gilson_encode_data(0, GSON_SINGLE, GSON_tSTRING, CAST_GIL chapa.sensor, 16, 0, 0);  // "sensor"
			// erro = gilson_encode_dataKV(0, GSON_SINGLE, GSON_tSTRING, "sensor", CAST_GIL chapa.sensor, 16, 0, 0);  // "sensor"

			cont_list_a = (uint16_t)va_arg(argptr, int);
		}
	}
	else if(tipo1 == GSON_LIST)
	{
		cont_list_a = (uint16_t)va_arg(argptr, int);
		if(tipo2 == GSON_tSTRING)
		{
			// erro = gilson_encode_data(2, GSON_LIST, GSON_tSTRING, CAST_GIL chapa.operadores, 2, 40, 0);  // "operadores"
			// erro = gilson_encode_dataKV(2, GSON_LIST, GSON_tSTRING, "operadores", CAST_GIL chapa.operadores, 2, 40, 0);  // "operadores"

			cont_list_b = (uint16_t)va_arg(argptr, int);
		}
	}
	else if(tipo1 == GSON_MTX2D)
	{
		// erro = gilson_encode_data(13, GSON_MTX2D, GSON_tFLOAT32, CAST_GIL chapa.acc, 2, 3, 3);  // "acc"
		// erro = gilson_encode_dataKV(13, GSON_MTX2D, GSON_tFLOAT32, "acc", CAST_GIL chapa.acc, 2, 3, 3);  // "acc"

		cont_list_a = (uint16_t)va_arg(argptr, int);
		cont_list_b = (uint16_t)va_arg(argptr, int);
		cont_list_step = (uint16_t)va_arg(argptr, int);
	}
	else
	{
		// erro
	}

	//printf("valor:%u\n", &valor);

	//printf("cont_list_a:%u, cont_list_b:%u, cont_list_step:%u\n", cont_list_a, cont_list_b, cont_list_step);

	va_end(argptr);


	return gilson_encode_data_base(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step);
}



//========================================================================================================================
//========================================================================================================================
//========================================================================================================================
//========================================================================================================================
// PARTE DE DECODIFICAÇÃO


int32_t gilson_decode_init(const uint8_t *pack, uint8_t *modo)
{
	SIZE_TYPE_RAM end_ram=0;
	uint32_t crc1=0, crc2=0;
	int32_t erro=erGSON_OK;

	// queremos decodificar um pacote...

	if(s_gil[ig].ativo==1)
	{
		erro = erGSON_15;
		end_ram = (SIZE_TYPE_RAM)pack;  // endereço na ram????

		// atual 'ig' está ativo ja... nao tem o que fazer... vamos consultar o segundo
		if(ig==0)
		{
			// vamos ver o segundo...
			if(s_gil[ig+1].ativo==1)
			{
				erro = erGSON_16;
				// nao tem o que fazer.... aborta a missao!!!!
			}
			else if(end_ram==s_gil[ig].end_ram)
			{
				// quer utililzar o mesmo vetor geral????? nao tem fazer isso....
				erro = erGSON_17;
				// nao tem o que fazer.... aborta a missao!!!!
			}
			else
			{
				erro = erGSON_OK;
				ig=1;  // vai para o segundo e esse passar ser o atual global da lib
			}
		}

		if(erro!=erGSON_OK) goto deu_erro;
	}

	memset(&s_gil[ig], 0x00, sizeof(s_gil[ig]));

	s_gil[ig].end_ram = (SIZE_TYPE_RAM)pack;  // endereço na ram????
	s_gil[ig].bufr = pack;
	s_gil[ig].modo = s_gil[ig].bufr[0];
	s_gil[ig].pos_bytes = 0;
	s_gil[ig].cont_itens = 0;

	if(s_gil[ig].modo >= GSON_MODO_MAX)
	{
		erro = erGSON_18;
		goto deu_erro;
	}
	else if(s_gil[ig].modo == GSON_MODO_ZIP || s_gil[ig].modo == GSON_MODO_KV_ZIP)
	{
		s_gil[ig].pos_bytes = OFFSET_MODO_ZIP;  // offset geral
	}
	else
	{
		memcpy(&crc1, &s_gil[ig].bufr[1], 4);
		memcpy(&s_gil[ig].pos_bytes2, &s_gil[ig].bufr[5], 2);
		s_gil[ig].cont_itens2 = s_gil[ig].bufr[7];

		crc2 = fs_crc(0xffffffff, &s_gil[ig].bufr[5], s_gil[ig].pos_bytes2-5);

		if(crc1 != crc2)
		{
			erro = erGSON_19;
			goto deu_erro;
			//return -3;
		}

		s_gil[ig].crc_out = crc1;  // atualiza o global

		s_gil[ig].pos_bytes = OFFSET_MODO_FULL;  // offset geral
	}

	deu_erro:

	if(erro==erGSON_OK)
	{
		s_gil[ig].ativo = 1;  // ativa decodificação
		*modo = s_gil[ig].modo;
	}
	else
	{
		s_gil[ig].erro = erro;
		s_gil[ig].ativo = 0;
	}

#if (USO_DEBUG_LIB==1)
	printf("DEBUG gilson_decode_init::: ig:%u, erro:%i  modo:%u  pos_bytes:%u  cont_itens:%u  cru:[%u,%u,%u,%u,%u,%u,%u,%u]  crc:%u==%u  pos_bytes2:%u  cont_itens2:%u\n", ig, erro, s_gil[ig].modo, s_gil[ig].pos_bytes, s_gil[ig].cont_itens, s_gil[ig].bufr[0], s_gil[ig].bufr[1], s_gil[ig].bufr[2], s_gil[ig].bufr[3], s_gil[ig].bufr[4], s_gil[ig].bufr[5], s_gil[ig].bufr[6], s_gil[ig].bufr[7], crc1, crc2, s_gil[ig].pos_bytes2, s_gil[ig].cont_itens2);
#endif

	return erro;
}


int32_t gilson_decode_valid(const uint8_t *pack)
{
	int32_t erro=erGSON_OK;
	uint8_t modo;

	erro = gilson_decode_init(pack, &modo);
	memset(&s_gil[ig], 0x00, sizeof(s_gil[ig]));  // limpa para liberar pois nao vamos continuar o decode...

	return erro;
}


int32_t gilson_decode_end(uint32_t *crc)
{
	int32_t erro=erGSON_OK;

	if(s_gil[ig].erro != erGSON_OK)
	{
		erro = s_gil[ig].erro;  // vamos manter sempre o mesmo erro!!!
		goto deu_erro;
	}

	if(s_gil[ig].modo == GSON_MODO_ZIP || s_gil[ig].modo == GSON_MODO_KV_ZIP)
	{
		s_gil[ig].crc_out = fs_crc(0xffffffff, s_gil[ig].bufr, s_gil[ig].pos_bytes);
	}
	else  // outros modos
	{
		// 's_gil[ig].crc_out' ja foi calculado em 'gilson_decode_init()'
		if(s_gil[ig].cont_itens == s_gil[ig].cont_itens2 && s_gil[ig].pos_bytes != s_gil[ig].pos_bytes2)
		{
			// chegou até o fim 'cont_itens2' mas o 'pos_bytes' nao bateu com o 'pos_bytes2', dai deu ruim
			erro = erGSON_20;
		}
		else
		{
			// indica que nao leu todas chaves ou nao foi até a última ou nao veu em ordem até o fim
			s_gil[ig].pos_bytes = s_gil[ig].pos_bytes2;
			s_gil[ig].cont_itens = s_gil[ig].cont_itens2;
		}
	}

	s_gil[ig].ativo = 0;

	*crc = s_gil[ig].crc_out;

	deu_erro:

#if (USO_DEBUG_LIB==1)
	printf("DEBUG gilson_decode_end::: ig:%u, erro:%i, modo:%u, pos_bytes:%u==%u, cont_itens:%u==%u, crc:%u \n", ig, erro, s_gil[ig].modo, s_gil[ig].pos_bytes, s_gil[ig].pos_bytes2, s_gil[ig].cont_itens, s_gil[ig].cont_itens2, s_gil[ig].crc_out);
#endif

	if(erro==erGSON_OK)
	{
		if(ig==1)
		{
			ig = 0;  // volta para o primeiro, pois está aberto!!!
			return s_gil[ig+1].pos_bytes;
		}
		else
		{
			return s_gil[ig].pos_bytes;
		}
	}
	else
	{
		s_gil[ig].erro = erro;
		return erro;
	}
}


// colocar sempre o "(uint8_t *)" na frente da variavel de entrada 'multi_data' e passar como "&"
int32_t gilson_decode_data_base(const uint8_t chave, char *nome_chave, const uint8_t tipo1, const uint8_t tipo2, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	int32_t erro=erGSON_OK, len;
	uint16_t i, vezes = 1, nbytes=1, temp16;

	if(s_gil[ig].erro != erGSON_OK)
	{
		erro = s_gil[ig].erro;  // vamos manter sempre o mesmo erro!!!
		goto deu_erro;
	}

	if(s_gil[ig].ativo==0)
	{
		erro = erGSON_21;
		goto deu_erro;
	}

	if(tipo1>=GSON_MAX || tipo2>=GSON_tMAX)
	{
		erro = erGSON_22;
		goto deu_erro;
	}

	if(chave > s_gil[ig].cont_itens)
	{
		// quer add uma chave maior do que a contagem crescente...
		// vamos varrer todas as chaves até achar a 'chave' desejada mas só funciona no modo 'GSON_MODO_FULL'!!!
		erro = erGSON_23;
		goto deu_erro;
	}


	if(s_gil[ig].modo == GSON_MODO_KV || s_gil[ig].modo == GSON_MODO_KV_ZIP)
	{
		// nome da chave
		len = s_gil[ig].bufr[s_gil[ig].pos_bytes];
		s_gil[ig].pos_bytes += 1;

		memcpy(nome_chave, &s_gil[ig].bufr[s_gil[ig].pos_bytes], len);
		s_gil[ig].pos_bytes += len;

		// len vai ser menor que 'LEN_MAX_CHAVE_NOME' caracteres...
	}

	if(s_gil[ig].modo == GSON_MODO_FULL || s_gil[ig].modo == GSON_MODO_KV)
	{
		//bufr[pos_bytes] = tipo_mux;
		s_gil[ig].pos_bytes += 1;
	}


	if(tipo1 == GSON_LIST)
	{
		if(cont_list_a == 0)
		{
			erro = erGSON_24;
			goto deu_erro;
		}
		vezes = cont_list_a;

		if(s_gil[ig].modo == GSON_MODO_FULL || s_gil[ig].modo == GSON_MODO_KV)
		{
			//memcpy(&bufr[pos_bytes], &vezes, 2);
			s_gil[ig].pos_bytes += 2;
		}

		if(tipo2==GSON_tSTRING)
		{
			if(cont_list_b==0)
			{
				// precisamos do offset cru da lista de strings...
				erro = erGSON_25;
				goto deu_erro;
			}
			else
			{
				if(s_gil[ig].modo == GSON_MODO_FULL || s_gil[ig].modo == GSON_MODO_KV)
				{
					//bufr[pos_bytes] = (uint8_t)cont_list_b;  // len_string_max
					s_gil[ig].pos_bytes += 1;
				}
			}
		}
	}
	else if(tipo1 == GSON_MTX2D)
	{
		if(cont_list_a == 0 || cont_list_b == 0  || cont_list_step == 0)
		{
			erro = erGSON_26;
			goto deu_erro;
		}
		//vezes = cont_list_a * cont_list_b;

		if(s_gil[ig].modo == GSON_MODO_FULL || s_gil[ig].modo == GSON_MODO_KV)
		{
			//bufr[pos_bytes] = (uint8_t)cont_list_a;
			s_gil[ig].pos_bytes += 1;
			//memcpy(&bufr[pos_bytes], &cont_list_b, 2);
			s_gil[ig].pos_bytes += 2;
			//memcpy(&bufr[pos_bytes], &cont_list_step, 2);
			s_gil[ig].pos_bytes += 2;
		}
	}
	else  // GSON_SINGLE
	{
		vezes = 1;
		/*
		if(tipo2==GSON_tSTRING)
		{
			if(cont_list_a==0)  // por mais que nao vá utilizar... para 1 string somente
			{
				erro = -9;
				goto deu_erro;
			}
		}
		*/
		// 'cont_list_b' só precisa na codificacao, agora é com base no 'len' da vez
	}


	switch(tipo2)
	{
	case GSON_tBIT:
		// nada ainda...
		break;
	case GSON_tINT8:
	case GSON_tUINT8:
		nbytes=1;
		break;
	case GSON_tINT16:
	case GSON_tUINT16:
		nbytes=2;
		break;
	case GSON_tINT32:
	case GSON_tUINT32:
		nbytes=4;
		break;
	case GSON_tINT64:
	case GSON_tUINT64:
		nbytes=8;
		break;
	case GSON_tFLOAT32:
		nbytes=4;
		break;
	case GSON_tFLOAT64:
		nbytes=8;
		break;
	case GSON_tSTRING:
		// só vai...
		break;
	default:
		erro = erGSON_27;
		goto deu_erro;
	}

	if(tipo2 == GSON_tSTRING)
	{
		for(i=0; i<vezes; i++)
		{
			len = s_gil[ig].bufr[s_gil[ig].pos_bytes];
			s_gil[ig].pos_bytes += 1;

			memcpy(valor+(i*cont_list_b), &s_gil[ig].bufr[s_gil[ig].pos_bytes], len);
			s_gil[ig].pos_bytes += len;
		}
	}
	else if(tipo2 == GSON_tBIT)
	{
		// nada ainda...
	}
	else
	{
		// para o resto é só varer bytes
		if(tipo1 == GSON_MTX2D)
		{
			// vamos ao exemplo:
			// float x[10][250] e na realidade vamos usar somente x[2][3] ==== x[cont_list_a][cont_list_b] com step de 'cont_list_step'
			// 10*250=2500 elementos serializados e como é float *4 = 10000 bytes, mas a serializacao vai ir [0][0], [0][1]...[0][249], [1][0] ... [9][249]
			// logo se eu quero x[2][3] tenho que dar os offset com base nos valores máximos (250 elementos cada linha)
			for(i=0; i<cont_list_a; i++)
			{
				temp16 = i*cont_list_step*nbytes;  // passos em cada linha da matriz
				memcpy(valor+temp16, &s_gil[ig].bufr[s_gil[ig].pos_bytes], nbytes*cont_list_b);
				s_gil[ig].pos_bytes += nbytes*cont_list_b;
			}
		}
		else
		{
			memcpy(valor, &s_gil[ig].bufr[s_gil[ig].pos_bytes], nbytes*vezes);
			s_gil[ig].pos_bytes += nbytes*vezes;
		}
	}


	deu_erro:

	if(erro==erGSON_OK)
	{
		s_gil[ig].cont_itens += 1;
	}
	else
	{
		s_gil[ig].erro = erro;
#if (USO_DEBUG_LIB==1)
		printf("DEBUG gilson_decode_data::: ig:%u, ERRO:%i modo:%u, tipo1:%u, tipo2:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u\n", ig, erro, s_gil[ig].modo, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step);
#endif
	}

	return erro;
}


int32_t gilson_decode_data(const uint8_t chave, const uint8_t tipo1, const uint8_t tipo2, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	char temp[4];
	return gilson_decode_data_base(chave, temp, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step);
}

int32_t gilson_decode_dataKV(const uint8_t chave, char *nome_chave, const uint8_t tipo1, const uint8_t tipo2, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	return gilson_decode_data_base(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step);
}


// só funciona para modo 'GSON_MODO_FULL'!!!!
int32_t gilson_decode_data_full_base(const uint8_t chave, char *nome_chave, uint8_t *valor)
{
	int32_t erro=erGSON_OK, len;
	uint16_t i, vezes = 1, cont_list_a=0, cont_list_b=0, cont_list_step=0, nbytes=1, temp16;  // j, init
	uint8_t tipo_mux=0, tipo1=255, tipo2=255, bypass=0;

	if(s_gil[ig].erro != erGSON_OK)
	{
		erro = s_gil[ig].erro;  // vamos manter sempre o mesmo erro!!!
		goto deu_erro;
	}

	if(s_gil[ig].ativo==0)
	{
		erro = erGSON_28;
		goto deu_erro;
	}

	// somente funciona no modo 'GSON_MODO_FULL' e 'GSON_MODO_KV'!!!!
	if(s_gil[ig].modo != GSON_MODO_FULL && s_gil[ig].modo != GSON_MODO_KV)
	{
		erro = erGSON_29;
		goto deu_erro;
	}

	if(chave >= s_gil[ig].cont_itens2)
	{
		// quer add uma chave maior do que a contagem total que é decodificada em decode_init e aloca em 'cont_itens2'
		// nesse caso nao tem como prosseguir!!!!
		erro = erGSON_30;
		goto deu_erro;
	}


	while(1)
	{
		if(chave > s_gil[ig].cont_itens)
		{
			// quer ler uma chave maior do que a contagem crescente... temos que ir para frente...
			// vai iniciar de onde parou até achar o que queremos
			bypass=1;
		}
		else if(chave < s_gil[ig].chave_atual)
		{
			// quer ler uma chave menor, que ja foi lida e/u passada... temos que ir para trás
			// vai partir de zero e vai até achar o que queremos
			bypass=1;
			s_gil[ig].cont_itens = 0;
			s_gil[ig].pos_bytes = OFFSET_MODO_FULL;
		}
		else
		{
			bypass=0;  // é a que estamos, está na sequencia crescente correta
			s_gil[ig].chave_atual = chave;  // salva a última feita ok
			s_gil[ig].cont_itens_old = s_gil[ig].cont_itens;
		}



		if(s_gil[ig].modo == GSON_MODO_KV)
		{
			// nome da chave
			len = s_gil[ig].bufr[s_gil[ig].pos_bytes];
			s_gil[ig].pos_bytes += 1;

			memcpy(nome_chave, &s_gil[ig].bufr[s_gil[ig].pos_bytes], len);
			s_gil[ig].pos_bytes += len;

			// len vai ser menor que 'LEN_MAX_CHAVE_NOME' caracteres...
		}


		tipo_mux = s_gil[ig].bufr[s_gil[ig].pos_bytes];
		s_gil[ig].pos_bytes += 1;

		// 0baaabbbbb = a:tipo1, b=tipo2
		//tipo_mux = tipo1<<5;
		//tipo_mux |= tipo2;
		tipo1 = tipo_mux>>5;
		tipo2 = tipo_mux&0b11111;
		//printf("decode: 0baaabbbbb = a:tipo1(%u), b=tipo2()%u tipo_mux:%u\n", tipo1, tipo2, tipo_mux);



		if(tipo1>=GSON_MAX || tipo2>=GSON_tMAX)
		{
			erro = erGSON_31;
			goto deu_erro;
		}

		if(tipo1 == GSON_LIST)
		{
			memcpy(&vezes, &s_gil[ig].bufr[s_gil[ig].pos_bytes], 2);
			s_gil[ig].pos_bytes += 2;

			if(vezes == 0)
			{
				erro = erGSON_32;
				goto deu_erro;
			}

			if(tipo2 == GSON_tSTRING)
			{
				cont_list_b = s_gil[ig].bufr[s_gil[ig].pos_bytes];
				s_gil[ig].pos_bytes += 1;

				if(cont_list_b==0)
				{
					// precisamos do offset cru da lista de strings...
					erro = erGSON_33;
					goto deu_erro;
				}
			}
		}
		else if(tipo1 == GSON_MTX2D)
		{
			cont_list_a = s_gil[ig].bufr[s_gil[ig].pos_bytes];
			s_gil[ig].pos_bytes += 1;
			memcpy(&cont_list_b, &s_gil[ig].bufr[s_gil[ig].pos_bytes], 2);
			s_gil[ig].pos_bytes += 2;
			memcpy(&cont_list_step, &s_gil[ig].bufr[s_gil[ig].pos_bytes], 2);
			s_gil[ig].pos_bytes += 2;

			//vezes = cont_list_a * cont_list_b;
			if(cont_list_a == 0 || cont_list_b == 0 || cont_list_step == 0)
			{
				erro = erGSON_34;
				goto deu_erro;
			}
		}
		else  // GSON_SINGLE
		{
			// ja alocou 'tipo2'...
			vezes = 1;

			/*
			if(tipo2==GSON_tSTRING)
			{
				if(cont_list_a==0)  // por mais que nao vá utilizar... para 1 string somente
				{
					erro = -9;
					goto deu_erro;
				}
			}
			*/
			// 'cont_list_a' só precisa na codificacao, agora é com base no 'len' da vez
		}


		switch(tipo2)
		{
		case GSON_tBIT:
			// nada ainda...
			break;
		case GSON_tINT8:
		case GSON_tUINT8:
			nbytes=1;
			break;
		case GSON_tINT16:
		case GSON_tUINT16:
			nbytes=2;
			break;
		case GSON_tINT32:
		case GSON_tUINT32:
			nbytes=4;
			break;
		case GSON_tINT64:
		case GSON_tUINT64:
			nbytes=8;
			break;
		case GSON_tFLOAT32:
			nbytes=4;
			break;
		case GSON_tFLOAT64:
			nbytes=8;
			break;
		case GSON_tSTRING:
			// só vai...
			break;
		default:
			erro = erGSON_35;
			goto deu_erro;
		}


		if(tipo2 == GSON_tSTRING)
		{
			for(i=0; i<vezes; i++)
			{
				len = s_gil[ig].bufr[s_gil[ig].pos_bytes];
				s_gil[ig].pos_bytes += 1;
				if(bypass == 0)
				{
					memcpy(valor+(i*cont_list_b), &s_gil[ig].bufr[s_gil[ig].pos_bytes], len);
				}
				s_gil[ig].pos_bytes += len;
			}
		}
		else if(tipo2 == GSON_tBIT)
		{
			// nada ainda...
		}
		else
		{
			// para o resto é só varer bytes
			if(tipo1 == GSON_MTX2D)
			{
				// vamos ao exemplo:
				// float x[10][250] e na realidade vamos usar somente x[2][3] ==== x[cont_list_a][cont_list_b] com step de 'cont_list_step'
				// 10*250=2500 elementos serializados e como é float *4 = 10000 bytes, mas a serializacao vai ir [0][0], [0][1]...[0][249], [1][0] ... [9][249]
				// logo se eu quero x[2][3] tenho que dar os offset com base nos valores máximos (250 elementos cada linha)
				for(i=0; i<cont_list_a; i++)
				{
					temp16 = i*cont_list_step*nbytes;  // passos em cada linha da matriz
					if(bypass == 0)
					{
						memcpy(valor+temp16, &s_gil[ig].bufr[s_gil[ig].pos_bytes], nbytes*cont_list_b);
					}
					s_gil[ig].pos_bytes += nbytes*cont_list_b;
				}
			}
			else
			{
				if(bypass == 0)
				{
					memcpy(valor, &s_gil[ig].bufr[s_gil[ig].pos_bytes], nbytes*vezes);
				}
				s_gil[ig].pos_bytes += nbytes*vezes;
			}
		}

		s_gil[ig].erro = erro;

		deu_erro:

		if(erro==erGSON_OK)
		{
			s_gil[ig].cont_itens += 1;
		}
		else
		{
	#if (USO_DEBUG_LIB==1)
			printf("DEBUG gilson_decode_data_full::: ig:%u, ERRO:%i tipo1:%u, tipo2:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u, chave:%u, cont_itens:%u\n", ig, erro, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step, chave, s_gil[ig].cont_itens);
	#endif
			break;
		}

		if(bypass == 0)
		{
			break;
		}
	}

	return erro;
}


int32_t gilson_decode_data_full(const uint8_t chave, uint8_t *valor)
{
	char temp[4];
	return gilson_decode_data_full_base(chave, temp, valor);
}


int32_t gilson_decode_dataKV_full(const uint8_t chave, char *nome_chave, uint8_t *valor)
{
	return gilson_decode_data_full_base(chave, nome_chave, valor);
}

int32_t gilson_decode(const uint8_t chave, ...)
{
	//int32_t erro=0;
	uint16_t cont_list_a=0, cont_list_b=0, cont_list_step=0;
	//uint16_t i, vezes = 1, nbytes = 1, temp16;
	uint8_t tipo1=255, tipo2=255, modofull=1;
	uint8_t *valor;
	char *nome_chave = {"\0"};
	va_list argptr;

	va_start(argptr, chave);

	if(s_gil[ig].modo == GSON_MODO_KV || s_gil[ig].modo == GSON_MODO_KV_ZIP)
	{
		nome_chave = va_arg(argptr, char *);
	}

	if(s_gil[ig].modo != GSON_MODO_FULL && s_gil[ig].modo != GSON_MODO_KV)
	{
		tipo1 = (uint8_t)va_arg(argptr, int);
		tipo2 = (uint8_t)va_arg(argptr, int);
		modofull = 0;
	}

	valor = va_arg(argptr, uint8_t *);

	if(modofull==0)
	{
		if(tipo1 == GSON_SINGLE)
		{
			if(tipo2 == GSON_tSTRING)
			{
				// erro = gilson_encode_data(0, GSON_SINGLE, GSON_tSTRING, CAST_GIL chapa.sensor, 16, 0, 0);  // "sensor"
				// erro = gilson_encode_dataKV(0, GSON_SINGLE, GSON_tSTRING, "sensor", CAST_GIL chapa.sensor, 16, 0, 0);  // "sensor"

				cont_list_a = (uint16_t)va_arg(argptr, int);
			}
		}
		else if(tipo1 == GSON_LIST)
		{
			cont_list_a = (uint16_t)va_arg(argptr, int);
			if(tipo2 == GSON_tSTRING)
			{
				// erro = gilson_encode_data(2, GSON_LIST, GSON_tSTRING, CAST_GIL chapa.operadores, 2, 40, 0);  // "operadores"
				// erro = gilson_encode_dataKV(2, GSON_LIST, GSON_tSTRING, "operadores", CAST_GIL chapa.operadores, 2, 40, 0);  // "operadores"

				cont_list_b = (uint16_t)va_arg(argptr, int);
			}
		}
		else if(tipo1 == GSON_MTX2D)
		{
			// erro = gilson_encode_data(13, GSON_MTX2D, GSON_tFLOAT32, CAST_GIL chapa.acc, 2, 3, 3);  // "acc"
			// erro = gilson_encode_dataKV(13, GSON_MTX2D, GSON_tFLOAT32, "acc", CAST_GIL chapa.acc, 2, 3, 3);  // "acc"

			cont_list_a = (uint16_t)va_arg(argptr, int);
			cont_list_b = (uint16_t)va_arg(argptr, int);
			cont_list_step = (uint16_t)va_arg(argptr, int);
		}
		else
		{
			// erro
		}
	}

	va_end(argptr);


	if(modofull==0)
	{
		return gilson_decode_data_base(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step);
	}
	else
	{
		return gilson_decode_data_full_base(chave, nome_chave, valor);
	}
}
