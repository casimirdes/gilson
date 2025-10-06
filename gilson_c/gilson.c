/*
 * gilson.c
 *
 *  Created on: 30 de out. de 2024
 *      Author: mella
 */


#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <float.h>

#include "gilson.h"

// fins de debug
#define TIPO_DEVICE			1  // 0=microcontrolador, 1=PC
#define USO_DEBUG_LIB		0  // ativa os prints debug, mais dedicado a PC
#define PRINT_DEBUG			0  // 1 = printa toda vida o debug

// SIZE_TYPE_RAM = como a RAM é 'medida/vista' no sistema: uint8_t, uint16_t, uint32_t, uint64_t, (ex.: PC de 64bits = uint64_t, STM32 = 32bits = uint32_t)

#if (TIPO_DEVICE==1)

#define SIZE_TYPE_RAM		uint64_t

// ...

#else

#define SIZE_TYPE_RAM		uint32_t

// ...

#endif  // #if (TIPO_DEVICE==1)



#define LEN_PACKS_GILSON	2  // 2 pacotes manipulaveis "ao mesmo tempo", se abriu o segundo tem que fechar para voltar para o primeiro!!!

#define OFFSET_MODO_ZIP		2
#define OFFSET_MODO_FULL	8

#define LEN_MAX_CHAVE_NOME	16  // tamanho máximo do nome chave de cada elemento ala JSON quando utilizado modo 'GSON_MODO_KV'

#define TIPO_GSON_LDIN		0b11100000  // lista dinâmica de dados de diversos tipos, limitado até 255 tipos e nao pode ter lista de lista
#define TIPO_GSON_NULL		0b11111111  // quando for entrar com uma data nula, vai chamar uma função específica para sinalizar que vai gravar a chave porem nao terá dados

#define LEN_MAX_STRING		255  // tamanho maximo de um item que é do tipo string, seja 'GSON_SINGLE' ou 'GSON_LIST'

enum e_TIPO_OPER  // uso de 'tipo_operacao'
{
	e_OPER_NULL,			// nenhuma ainda...
	e_OPER_ENCODE,  		// operação de encode()
	e_OPER_DECODE,  		// operação de decode()
};

// estrutura de controle da lib gilson
typedef struct {
	SIZE_TYPE_RAM end_ram;
	uint32_t crc;
	uint32_t crc_out;  // quando decodifica
	int32_t erro;  // backup de erro geral de cada operacao...
	uint16_t pos_bytes, pos_bytes2, size_max_pack;  // pos_bytes_bk
	uint16_t pos_tipo_dl_init, pos_tipo_dl_end, cont_tipo_dinamico, pos_bytes_dl;  // salva posicao do buffer de w/r onde inicia esse tipo_dinamico
	uint8_t modo;  // 0=padrao, 1=compacto
	uint8_t ativo, tipo_operacao;
	uint8_t cont_itens, cont_itens2, cont_itens_old;  // contagem e monitoramento das chaves
	uint8_t chave_atual;  // para fins de comparar e validar chaves durante encode
	uint8_t tipo_dinamico, tam_list, nitens, tam_list2, nitens2, chave_dl;  // 0=nao trata, 1=está tratando desse tipo
	uint8_t chaves_null;  // até 255 chaves nulas, meio improvável...
	uint8_t *bufw;  // buffer de escrita/leitura
	const uint8_t *bufr;  // buffer somente leitura
}struct_gilson;


static struct_gilson s_gil[LEN_PACKS_GILSON]={0};
static uint8_t ig=0;


// copiado da lib do littlefs "lfs_crc()"
// Software CRC implementation with small lookup table
static uint32_t gil_crc(uint32_t crc, const uint8_t *buffer, const uint16_t size)
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
				// quer utililzar o mesmo buffer geral na RAM????? nao tem fazer isso...
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

	if(modo_ == GSON_MODO_JSON)
	{
		erro = erGSON_eJSON;
		goto deu_erro;
	}

	s_gil[ig].tipo_operacao = e_OPER_ENCODE;
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
#if (TIPO_DEVICE==0)
		printf_DEBUG("DEBUG gilson_encode_init::: ig:%u, erro:%i, end_ram:%X, modo:%u, pos_bytes:%u, cont_itens:%u, end_ram:%u|%u\n", ig, erro, s_gil[ig].end_ram, s_gil[ig].modo, s_gil[ig].pos_bytes, s_gil[ig].cont_itens, s_gil[ig].end_ram, end_ram);
#else  // PC
		printf("DEBUG gilson_encode_init::: ig:%u, erro:%i, end_ram:%X, modo:%u, pos_bytes:%u, cont_itens:%u, end_ram:%u|%u\n", ig, erro, s_gil[ig].end_ram, s_gil[ig].modo, s_gil[ig].pos_bytes, s_gil[ig].cont_itens, s_gil[ig].end_ram, end_ram);
#endif  // #if (TIPO_DEVICE==1)
#endif  // #if (USO_DEBUG_LIB==1)

	s_gil[ig].ativo = 1;  // ativa a estrutura 'ig' da vez!!!
	return erro;
}

// calcula e retorna crc independente do 'modo'
static int32_t gilson_encode_end_base(const uint8_t flag_crc, uint32_t *crc)
{
	int32_t erro=erGSON_OK;
	s_gil[ig].crc=0;


	// MODO ZIP: 8 bytes de offset geral:
	// [0] 1b = modo
	// [1] 1b = quantos elementos geral (nao soma as listas, trata como 1) até 255 elementos/itens
	// [2::] data

	// MODO FULL: 8 bytes de offset geral:
	// [0] 1b = modo
	// [1:4] 4b = crc pacote (crc32)
	// [5:6] 2b = tamanho pacote (até 65355 bytes)
	// [7] 1b = quantos elementos geral (nao soma as listas, trata como 1) até 255 elementos/itens
	// [8::] data

	if(s_gil[ig].erro != erGSON_OK || s_gil[ig].tipo_operacao != e_OPER_ENCODE)
	{
		if(s_gil[ig].erro == erGSON_OK)
		{
			s_gil[ig].erro = erGSON_OPER;
		}
		erro = s_gil[ig].erro;  // vamos manter sempre o mesmo erro!!!
		s_gil[ig].pos_bytes = 0;  // pra garantir que não deu nada...
		s_gil[ig].modo = 255;  // deu ruim!!!
		s_gil[ig].bufw[0] = s_gil[ig].modo;
	}
	else
	{
		s_gil[ig].bufw[0] = s_gil[ig].modo;

		if(s_gil[ig].modo == GSON_MODO_ZIP || s_gil[ig].modo == GSON_MODO_KV_ZIP)
		{
			s_gil[ig].bufw[1] = s_gil[ig].cont_itens;
			if(flag_crc)
			{
				s_gil[ig].crc = gil_crc(0xffffffff, s_gil[ig].bufw, s_gil[ig].pos_bytes);  // chamar depois que 'pos_bytes' está completo!!!!
			}
		}
		else  // outros modos
		{
			memcpy(&s_gil[ig].bufw[5], &s_gil[ig].pos_bytes, 2);
			s_gil[ig].bufw[7] = s_gil[ig].cont_itens;

			s_gil[ig].crc = gil_crc(0xffffffff, &s_gil[ig].bufw[5], s_gil[ig].pos_bytes-5);

			memcpy(&s_gil[ig].bufw[1], &s_gil[ig].crc, 4);
		}
	}


#if (USO_DEBUG_LIB==1)
#if (TIPO_DEVICE==0)
	printf_DEBUG("DEBUG gilson_encode_end_crc::: erro:%i, ig:%u, modo:%u, pos_bytes:%u, cont_itens:%u, crc:%u, chaves_null:%u, cru:[%u,%u,%u,%u,%u,%u,%u,%u]\n", s_gil[ig].erro, ig, s_gil[ig].modo, s_gil[ig].pos_bytes, s_gil[ig].cont_itens, s_gil[ig].crc, s_gil[ig].chaves_null, s_gil[ig].bufw[0], s_gil[ig].bufw[1], s_gil[ig].bufw[2], s_gil[ig].bufw[3], s_gil[ig].bufw[4], s_gil[ig].bufw[5], s_gil[ig].bufw[6], s_gil[ig].bufw[7]);
#else  // PC
	printf("DEBUG gilson_encode_end_crc::: erro:%i, ig:%u, modo:%u, pos_bytes:%u, cont_itens:%u, crc:%u, chaves_null:%u, cru:[%u,%u,%u,%u,%u,%u,%u,%u]\n", s_gil[ig].erro, ig, s_gil[ig].modo, s_gil[ig].pos_bytes, s_gil[ig].cont_itens, s_gil[ig].crc, s_gil[ig].chaves_null, s_gil[ig].bufw[0], s_gil[ig].bufw[1], s_gil[ig].bufw[2], s_gil[ig].bufw[3], s_gil[ig].bufw[4], s_gil[ig].bufw[5], s_gil[ig].bufw[6], s_gil[ig].bufw[7]);
#endif  // #if (TIPO_DEVICE==1)
#endif  // #if (USO_DEBUG_LIB==1)

	// por mais que deu algum erro ou está em erro, vamos limpar a estrutura e sair fora...
	s_gil[ig].ativo = 0;
	s_gil[ig].tipo_operacao = e_OPER_NULL;

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


int32_t gilson_encode_end(void)
{
	uint32_t crc=0;  // não usa...
	return gilson_encode_end_base(0, &crc);
}


int32_t gilson_encode_end_crc(uint32_t *crc)
{
	return gilson_encode_end_base(1, crc);
}



// colocar sempre o "(uint8_t *)" na frente da variavel de entrada 'multi_data' e passar como "&"
static int32_t gilson_encode_data_base(const uint8_t chave, const char *nome_chave, const uint8_t tipo1, const uint8_t tipo2, const uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	int32_t erro=erGSON_OK, len=0;  // check
	uint16_t i, vezes = 1, nbytes = 1, temp16, pos_bytes_check=0;
	uint8_t len_string_max=0, tipo_mux=0, flag_teste=0;
	char buff[LEN_MAX_STRING];  // supondo tamanho maximo de strings

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

	if(s_gil[ig].tipo_operacao!=e_OPER_ENCODE)
	{
		erro = erGSON_OPER;
		goto deu_erro;
	}

	if(tipo1!=255 && tipo2!=255)
	{
		if(tipo1>=GSON_MAX)
		{
			erro = erGSON_4;
			goto deu_erro;
		}
		if(tipo2>=GSON_tMAX)
		{
			erro = erGSON_4b;
			goto deu_erro;
		}
	}
	else
	{
		if(s_gil[ig].modo == GSON_MODO_ZIP || s_gil[ig].modo == GSON_MODO_KV_ZIP)
		{
			// esquema de nulo nao aceita em modo ZIP!!
			erro = erGSON_NULL;
			goto deu_erro;
		}
	}
	// else (tipo1==255 && tipo2==255) temos um caso NULL de chave!!!

	if(chave > s_gil[ig].cont_itens)
	{
		// quer add uma chave maior do que a contagem crescente... nao tem como
		erro = erGSON_5;
		goto deu_erro;
	}

	if(s_gil[ig].cont_itens >= LIMIT_GSON_KEYS)
	{
		erro = erGSON_LIMKEY;
		goto deu_erro;
	}

	if(s_gil[ig].cont_itens>0 && chave <= s_gil[ig].chave_atual && s_gil[ig].tipo_dinamico==0)
	{
		erro = erGSON_SMKEYe;
		goto deu_erro;
	}

	// varifica quantos bytes vai precisar para essa chave...
	flag_teste = 1;  // parte em 1 pois vai fazer o teste primeiro... depois retorna e mete bala na real da vida
	//pos_bytes_bk = s_gil[ig].pos_bytes;  // faz o backup atual
	pos_bytes_check = s_gil[ig].pos_bytes;
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
			if(flag_teste==0 && s_gil[ig].tipo_dinamico==0)
			{
				s_gil[ig].bufw[s_gil[ig].pos_bytes] = (uint8_t)len;
				s_gil[ig].pos_bytes += 1;
			}
			pos_bytes_check += 1;

			if(flag_teste==0 && s_gil[ig].tipo_dinamico==0)
			{
				memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], nome_chave, len);
				s_gil[ig].pos_bytes += len;
			}
			pos_bytes_check += len;
		}
	}

	// 0baaabbbbb = a:tipo1, b=tipo2
	tipo_mux = tipo1<<5;
	tipo_mux |= tipo2;
	//printf("encode: 0baaabbbbb = a:tipo1(%u), b=tipo2()%u tipo_mux:%u\n", tipo1, tipo2, tipo_mux);

	if(s_gil[ig].modo == GSON_MODO_FULL || s_gil[ig].modo == GSON_MODO_KV)
	{
		if(flag_teste==0 && s_gil[ig].tipo_dinamico==0)
		{
			s_gil[ig].bufw[s_gil[ig].pos_bytes] = tipo_mux;
			s_gil[ig].pos_bytes += 1;
		}
		pos_bytes_check += 1;
	}


	if(tipo_mux != TIPO_GSON_NULL)
	{
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
				if(flag_teste==0 && s_gil[ig].tipo_dinamico==0)
				{
					memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], &vezes, 2);
					s_gil[ig].pos_bytes += 2;
				}
				pos_bytes_check += 2;
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
				else if(cont_list_b >= LEN_MAX_STRING)
				{
					erro = erGSON_9b;
					goto deu_erro;
				}
				else
				{
					len_string_max = cont_list_b;
					if(s_gil[ig].modo == GSON_MODO_FULL || s_gil[ig].modo == GSON_MODO_KV)
					{
						if(flag_teste==0 && s_gil[ig].tipo_dinamico==0)
						{
							s_gil[ig].bufw[s_gil[ig].pos_bytes] = len_string_max;
							s_gil[ig].pos_bytes += 1;
						}
						pos_bytes_check += 1;
					}
				}
				// cont_list_b > LEN_MAX_STRING_DATA ????
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
				if(flag_teste==0 && s_gil[ig].tipo_dinamico==0)
				{
					s_gil[ig].bufw[s_gil[ig].pos_bytes] = (uint8_t)cont_list_a;
					s_gil[ig].pos_bytes += 1;
				}
				pos_bytes_check += 1;

				if(flag_teste==0 && s_gil[ig].tipo_dinamico==0)
				{
					memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], &cont_list_b, 2);
					s_gil[ig].pos_bytes += 2;
				}
				pos_bytes_check += 2;

				if(flag_teste==0 && s_gil[ig].tipo_dinamico==0)
				{
					memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], &cont_list_step, 2);
					s_gil[ig].pos_bytes += 2;
				}
				pos_bytes_check += 2;
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
				else if(cont_list_a >= LEN_MAX_STRING)
				{
					erro = erGSON_12b;
					goto deu_erro;
				}
				else
				{
					len_string_max = cont_list_a;
				}
				// cont_list_a > LEN_MAX_STRING_DATA ????
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

				if(len<=LEN_MAX_STRING)
				{
					if(flag_teste==0)
					{
						s_gil[ig].bufw[s_gil[ig].pos_bytes] = (uint8_t)len;
						s_gil[ig].pos_bytes += 1;
					}
					pos_bytes_check += 1;

					if(flag_teste==0)
					{
						memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], buff, len);
						s_gil[ig].pos_bytes += len;
					}
					pos_bytes_check += len;
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
						s_gil[ig].pos_bytes += nbytes*cont_list_b;
					}
					pos_bytes_check += nbytes*cont_list_b;
				}
			}
			else
			{
				if(flag_teste==0)
				{
					memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], valor, nbytes*vezes);
					s_gil[ig].pos_bytes += nbytes*vezes;
				}
				pos_bytes_check += nbytes*vezes;
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
	}
	else
	{
		if(flag_teste==1)  // ou em 0 ou em 1, mas nao nos 2 pois vai duplicar
		{
			s_gil[ig].chaves_null+=1;
		}
	}
	// else (tipo_mux == TIPO_GSON_NULL)  so grava o 'tipo_mux' e cai fora...

	deu_erro:

	if(flag_teste)
	{
		if(erro!=erGSON_OK || (pos_bytes_check > s_gil[ig].size_max_pack))
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
			pos_bytes_check = 0;  // só pra garantir o brique de nao explodir
			//s_gil[ig].pos_bytes = pos_bytes_bk;  // restaura backup atual
			goto goto_meta_bala;
		}
	}

	if(erro==erGSON_OK)
	{
		if(s_gil[ig].tipo_dinamico==0)
		{
			s_gil[ig].cont_itens += 1;
		}

		s_gil[ig].chave_atual = chave;
	}
	else
	{
		s_gil[ig].erro = erro;
	}


#if (USO_DEBUG_LIB==1)
	if(erro!=erGSON_OK)
	{
#if (TIPO_DEVICE==0)
		printf_DEBUG("DEBUG gilson_encode_data::: ig:%u, ERRO:%i, modo:%u, chave:%u, tipo1:%u, tipo2:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u, flag_teste:%u, tipo_dinamico:%u, pos_bytes_check:%u|%u, chaves_null:%u\n", ig, erro, s_gil[ig].modo, chave, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step, flag_teste, s_gil[ig].tipo_dinamico, pos_bytes_check, s_gil[ig].size_max_pack, s_gil[ig].chaves_null);
#else  // PC
		printf("DEBUG gilson_encode_data::: ig:%u, ERRO:%i, modo:%u, chave:%u, tipo1:%u, tipo2:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u, flag_teste:%u, tipo_dinamico:%u, pos_bytes_check:%u|%u, chaves_null:%u\n", ig, erro, s_gil[ig].modo, chave, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step, flag_teste, s_gil[ig].tipo_dinamico, pos_bytes_check, s_gil[ig].size_max_pack, s_gil[ig].chaves_null);
#endif  // #if (TIPO_DEVICE==1)
	}
#endif  // #if (USO_DEBUG_LIB==1)

	return erro;
}


int32_t gilson_encode_data(const uint8_t chave, const uint8_t tipo1, const uint8_t tipo2, const uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	//printf("gilson_encode_data::: chave:%u, tipo1:%u, tipo2:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u\n", chave, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step);

	return gilson_encode_data_base(chave, "x", tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step);
}

int32_t gilson_encode_dataKV(const uint8_t chave, const uint8_t tipo1, const uint8_t tipo2, char *nome_chave, const uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	return gilson_encode_data_base(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step);
}




//==========================================================================================================================================
// funcoes auxiliares fortemente tipadas de encode

//========================================= GSON_SINGLE
int32_t gilson_encode_u8(const uint8_t chave, const uint8_t valor)
{
	return gilson_encode_data_base(chave, "x", GSON_SINGLE, GSON_tUINT8, CAST_GIL valor, 0, 0, 0);
}

int32_t gilson_encode_s8(const uint8_t chave, const int8_t valor)
{
	return gilson_encode_data_base(chave, "x", GSON_SINGLE, GSON_tINT8, CAST_GIL valor, 0, 0, 0);
}

int32_t gilson_encode_u16(const uint8_t chave, const uint16_t valor)
{
	return gilson_encode_data_base(chave, "x", GSON_SINGLE, GSON_tUINT16, CAST_GIL valor, 0, 0, 0);
}

int32_t gilson_encode_s16(const uint8_t chave, const int16_t valor)
{
	return gilson_encode_data_base(chave, "x", GSON_SINGLE, GSON_tINT16, CAST_GIL valor, 0, 0, 0);
}

int32_t gilson_encode_u32(const uint8_t chave, const uint32_t valor)
{
	return gilson_encode_data_base(chave, "x", GSON_SINGLE, GSON_tUINT32, CAST_GIL valor, 0, 0, 0);
}

int32_t gilson_encode_s32(const uint8_t chave, const int32_t valor)
{
	return gilson_encode_data_base(chave, "x", GSON_SINGLE, GSON_tINT32, CAST_GIL valor, 0, 0, 0);
}

int32_t gilson_encode_u64(const uint8_t chave, const uint64_t valor)
{
	return gilson_encode_data_base(chave, "x", GSON_SINGLE, GSON_tUINT64, CAST_GIL valor, 0, 0, 0);
}

int32_t gilson_encode_s64(const uint8_t chave, const int64_t valor)
{
	return gilson_encode_data_base(chave, "x", GSON_SINGLE, GSON_tINT64, CAST_GIL valor, 0, 0, 0);
}

int32_t gilson_encode_f32(const uint8_t chave, const float valor)
{
	return gilson_encode_data_base(chave, "x", GSON_SINGLE, GSON_tFLOAT32, CAST_GIL valor, 0, 0, 0);
}

int32_t gilson_encode_f64(const uint8_t chave, const double valor)
{
	return gilson_encode_data_base(chave, "x", GSON_SINGLE, GSON_tFLOAT64, CAST_GIL valor, 0, 0, 0);
}

int32_t gilson_encode_str(const uint8_t chave, const char *valor, const uint16_t cont_list_a)
{
	return gilson_encode_data_base(chave, "x", GSON_SINGLE, GSON_tSTRING, (const uint8_t *)valor, cont_list_a, 0, 0);
}

//========================================= GSON_LIST
int32_t gilson_encode_lu8(const uint8_t chave, const uint8_t valor[], const uint16_t cont_list_a)
{
	return gilson_encode_data_base(chave, "x", GSON_LIST, GSON_tUINT8, (const uint8_t *)valor, cont_list_a, 0, 0);
}

int32_t gilson_encode_ls8(const uint8_t chave, const int8_t valor[], const uint16_t cont_list_a)
{
	return gilson_encode_data_base(chave, "x", GSON_LIST, GSON_tINT8, (const uint8_t *)valor, cont_list_a, 0, 0);
}

int32_t gilson_encode_lu16(const uint8_t chave, const uint16_t valor[], const uint16_t cont_list_a)
{
	return gilson_encode_data_base(chave, "x", GSON_LIST, GSON_tUINT16, (const uint8_t *)valor, cont_list_a, 0, 0);
}

int32_t gilson_encode_ls16(const uint8_t chave, const int16_t valor[], const uint16_t cont_list_a)
{
	return gilson_encode_data_base(chave, "x", GSON_LIST, GSON_tINT16, (const uint8_t *)valor, cont_list_a, 0, 0);
}

int32_t gilson_encode_lu32(const uint8_t chave, const uint32_t valor[], const uint16_t cont_list_a)
{
	return gilson_encode_data_base(chave, "x", GSON_LIST, GSON_tUINT32, (const uint8_t *)valor, cont_list_a, 0, 0);
}

int32_t gilson_encode_ls32(const uint8_t chave, const int32_t valor[], const uint16_t cont_list_a)
{
	return gilson_encode_data_base(chave, "x", GSON_LIST, GSON_tINT32, (const uint8_t *)valor, cont_list_a, 0, 0);
}

int32_t gilson_encode_lu64(const uint8_t chave, const uint64_t valor[], const uint16_t cont_list_a)
{
	return gilson_encode_data_base(chave, "x", GSON_LIST, GSON_tUINT64, (const uint8_t *)valor, cont_list_a, 0, 0);
}

int32_t gilson_encode_ls64(const uint8_t chave, const int64_t valor[], const uint16_t cont_list_a)
{
	return gilson_encode_data_base(chave, "x", GSON_LIST, GSON_tINT64, (const uint8_t *)valor, cont_list_a, 0, 0);
}

int32_t gilson_encode_lf32(const uint8_t chave, const float valor[], const uint16_t cont_list_a)
{
	return gilson_encode_data_base(chave, "x", GSON_LIST, GSON_tFLOAT32, (const uint8_t *)valor, cont_list_a, 0, 0);
}

int32_t gilson_encode_lf64(const uint8_t chave, const double valor[], const uint16_t cont_list_a)
{
	return gilson_encode_data_base(chave, "x", GSON_LIST, GSON_tFLOAT64, (const uint8_t *)valor, cont_list_a, 0, 0);
}

int32_t gilson_encode_lstr(const uint8_t chave, const char *valor, const uint16_t cont_list_a, const uint16_t cont_list_b)
{
	return gilson_encode_data_base(chave, "x", GSON_LIST, GSON_tSTRING, (const uint8_t *)valor, cont_list_a, cont_list_b, 0);
}

//========================================= GSON_MTX2D
int32_t gilson_encode_mu8(const uint8_t chave, const uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	return gilson_encode_data_base(chave, "x", GSON_MTX2D, GSON_tUINT8, (const uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step);
}

int32_t gilson_encode_ms8(const uint8_t chave, const int8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	return gilson_encode_data_base(chave, "x", GSON_MTX2D, GSON_tINT8, (const uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step);
}

int32_t gilson_encode_mu16(const uint8_t chave, const uint16_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	return gilson_encode_data_base(chave, "x", GSON_MTX2D, GSON_tUINT16, (const uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step);
}

int32_t gilson_encode_ms16(const uint8_t chave, const int16_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	return gilson_encode_data_base(chave, "x", GSON_MTX2D, GSON_tINT16, (const uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step);
}

int32_t gilson_encode_mu32(const uint8_t chave, const uint32_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	return gilson_encode_data_base(chave, "x", GSON_MTX2D, GSON_tUINT32, (const uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step);
}

int32_t gilson_encode_ms32(const uint8_t chave, const int32_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	return gilson_encode_data_base(chave, "x", GSON_MTX2D, GSON_tINT32, (const uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step);
}

int32_t gilson_encode_mu64(const uint8_t chave, const uint64_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	return gilson_encode_data_base(chave, "x", GSON_MTX2D, GSON_tUINT64, (const uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step);
}

int32_t gilson_encode_ms64(const uint8_t chave, const int64_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	return gilson_encode_data_base(chave, "x", GSON_MTX2D, GSON_tINT64, (const uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step);
}

int32_t gilson_encode_mf32(const uint8_t chave, const float *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	return gilson_encode_data_base(chave, "x", GSON_MTX2D, GSON_tFLOAT32, (const uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step);
}

int32_t gilson_encode_mf64(const uint8_t chave, const double *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	return gilson_encode_data_base(chave, "x", GSON_MTX2D, GSON_tFLOAT64, (const uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step);
}


//==========================================================================================================================================





//==========================================================================================================================================
int32_t gilson_encode_dl_init(const uint8_t chave, const uint8_t tam_list, const uint8_t nitens)
{
	int32_t erro=erGSON_OK;

	// OBS: nao foi testado ainda em modo KV

	if(s_gil[ig].erro != erGSON_OK)
	{
		erro = s_gil[ig].erro;  // vamos manter sempre o mesmo erro!!!
		goto deu_erro;
	}

	if(s_gil[ig].ativo==0)
	{
		erro = erGSON_36;
		goto deu_erro;
	}

	if(s_gil[ig].tipo_dinamico == 1)
	{
		// ja esta ativo esse modo... tem que teminar antes para iniciar um novo
		erro = erGSON_37;
		goto deu_erro;
	}

	if(chave > s_gil[ig].cont_itens)
	{
		// quer add uma chave maior do que a contagem crescente... nao tem como
		erro = erGSON_38;
		goto deu_erro;
	}

	//if(s_gil[ig].modo == GSON_MODO_KV || s_gil[ig].modo == GSON_MODO_KV_ZIP)
	// aloca nome da chave!!! ainda nao tem....

	s_gil[ig].tipo_operacao = e_OPER_ENCODE;
	s_gil[ig].tipo_dinamico = 1;  // vamos tratar um tipo dinamico
	s_gil[ig].pos_tipo_dl_init = s_gil[ig].pos_bytes;
	s_gil[ig].tam_list = tam_list;
	s_gil[ig].nitens = nitens;
	s_gil[ig].tam_list2 = 0;
	s_gil[ig].nitens2 = 0;
	s_gil[ig].cont_tipo_dinamico = 0;

	if(s_gil[ig].modo == GSON_MODO_FULL || s_gil[ig].modo == GSON_MODO_KV)
	{
		// lembrando que no padrao normal agora viria o 'tipo_mux'
		// 0baaabbbbb = a:tipo1, b=tipo2

		s_gil[ig].bufw[s_gil[ig].pos_bytes] = TIPO_GSON_LDIN;
		s_gil[ig].pos_bytes += 1;

		s_gil[ig].bufw[s_gil[ig].pos_bytes] = tam_list;
		s_gil[ig].pos_bytes += 1;

		s_gil[ig].bufw[s_gil[ig].pos_bytes] = nitens;
		s_gil[ig].pos_bytes += 1;
	}
	else
	{
		// muita gambi para fazer funcionar... chato pacassss, um dia quem sabe...
		erro = erGSON_63;
	}

	deu_erro:

	if(erro==erGSON_OK)
	{
		s_gil[ig].cont_itens += 1;
	}
	else
	{
		s_gil[ig].erro = erro;
	}


#if (USO_DEBUG_LIB==1)
#if (TIPO_DEVICE==0)
	printf_DEBUG("DEBUG gilson_encode_dl_init::: ig:%u, ERRO:%i, modo:%u, chave:%u, tam_list:%u, nitens:%u\n", ig, erro, s_gil[ig].modo, chave, tam_list, nitens);
#else  // PC
	printf("DEBUG gilson_encode_dl_init::: ig:%u, ERRO:%i, modo:%u, chave:%u, tam_list:%u, nitens:%u\n", ig, erro, s_gil[ig].modo, chave, tam_list, nitens);
#endif  // #if (TIPO_DEVICE==1)
#endif  // #if (USO_DEBUG_LIB==1)

	return erro;
}


static int32_t valid_gilson_encode_dl(const uint8_t item, const uint8_t tipo1, const uint8_t tipo2, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	int32_t erro=erGSON_OK;

	if(s_gil[ig].erro != erGSON_OK)
	{
		erro = s_gil[ig].erro;  // vamos manter sempre o mesmo erro!!!
		goto deu_erro;
	}

	if(s_gil[ig].ativo==0)
	{
		erro = erGSON_39;
		goto deu_erro;
	}

	if(s_gil[ig].tipo_dinamico == 0)
	{
		erro = erGSON_40;
		goto deu_erro;
	}

	if(s_gil[ig].tipo_operacao!=e_OPER_ENCODE)
	{
		erro = erGSON_OPER;
		goto deu_erro;
	}

	if(tipo1>=GSON_MAX)
	{
		erro = erGSON_41;
		goto deu_erro;
	}

	if(tipo2>=GSON_tMAX)
	{
		erro = erGSON_41b;
		goto deu_erro;
	}

	if(item > s_gil[ig].nitens)
	{
		// quer add uma item maior do que a contagem crescente... nao tem como
		erro = erGSON_42;
		goto deu_erro;
	}

	if(tipo1 == GSON_LIST)
	{
		if(cont_list_a == 0)
		{
			erro = erGSON_43;
			goto deu_erro;
		}


		if(tipo2==GSON_tSTRING)
		{
			// aqui 'cont_list_b' é tratado como uint8
			if(cont_list_b==0)
			{
				// precisamos do offset cru da lista de strings...
				erro = erGSON_44;
				goto deu_erro;
			}
		}
	}
	else if(tipo1 == GSON_MTX2D)
	{
		// aqui 'cont_list_b' é tratado como uint16
		if(tipo2==GSON_tSTRING)
		{
			// nao testado isso ainda, vai da ruim
			erro = erGSON_45;
			goto deu_erro;
		}

		if(cont_list_a == 0 || cont_list_b == 0 || cont_list_step == 0)
		{
			erro = erGSON_46;
			goto deu_erro;
		}
		//vezes = cont_list_a * cont_list_b;  nao utiliza...
	}
	else  // GSON_SINGLE
	{
		if(tipo2==GSON_tSTRING)
		{
			if(cont_list_a==0)
			{
				erro = erGSON_47;
				goto deu_erro;
			}
		}
	}

	deu_erro:

	return erro;
}


int32_t gilson_encode_dl_add(const uint8_t item, const uint8_t tipo1, const uint8_t tipo2, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	int32_t erro=erGSON_OK;
	uint16_t vezes = 1;
	uint8_t tipo_mux=0, len_string_max=0;


	erro = valid_gilson_encode_dl(item, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step);
	if(erro != erGSON_OK)
	{
		goto deu_erro;
	}

	// 0baaabbbbb = a:tipo1, b=tipo2
	tipo_mux = tipo1<<5;
	tipo_mux |= tipo2;
	//printf("encode: 0baaabbbbb = a:tipo1(%u), b=tipo2()%u tipo_mux:%u\n", tipo1, tipo2, tipo_mux);

	if(s_gil[ig].modo == GSON_MODO_FULL || s_gil[ig].modo == GSON_MODO_KV)
	{
		s_gil[ig].bufw[s_gil[ig].pos_bytes] = tipo_mux;
		s_gil[ig].pos_bytes += 1;
	}

	if(tipo1 == GSON_LIST)
	{
		vezes = cont_list_a;

		if(s_gil[ig].modo == GSON_MODO_FULL || s_gil[ig].modo == GSON_MODO_KV)
		{
			memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], &vezes, 2);
			s_gil[ig].pos_bytes += 2;
			//printf("DEBUG gilson_encode_data: [%u,%u,%u,%u]\n", bufw[pos_bytes-4], bufw[pos_bytes-3], bufw[pos_bytes-2], bufw[pos_bytes-1]);
		}

		if(tipo2==GSON_tSTRING)
		{
			// aqui 'cont_list_b' é tratado como uint8
			len_string_max = cont_list_b;
			if(s_gil[ig].modo == GSON_MODO_FULL || s_gil[ig].modo == GSON_MODO_KV)
			{
				s_gil[ig].bufw[s_gil[ig].pos_bytes] = len_string_max;
				s_gil[ig].pos_bytes += 1;
			}

		}
	}
	else if(tipo1 == GSON_MTX2D)
	{
		//vezes = cont_list_a * cont_list_b;  nao utiliza...

		if(s_gil[ig].modo == GSON_MODO_FULL || s_gil[ig].modo == GSON_MODO_KV)
		{
			s_gil[ig].bufw[s_gil[ig].pos_bytes] = (uint8_t)cont_list_a;
			s_gil[ig].pos_bytes += 1;

			memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], &cont_list_b, 2);
			s_gil[ig].pos_bytes += 2;

			memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], &cont_list_step, 2);
			s_gil[ig].pos_bytes += 2;
			//printf("DEBUG gilson_encode_data: [%u,%u,%u,%u]\n", bufw[pos_bytes-4], bufw[pos_bytes-3], bufw[pos_bytes-2], bufw[pos_bytes-1]);
		}
	}
	else  // GSON_SINGLE
	{
		vezes = 1;

		if(tipo2==GSON_tSTRING)
		{
			len_string_max = cont_list_a;
		}
	}

	deu_erro:

	if(erro!=erGSON_OK)
	{
		s_gil[ig].erro = erro;

#if (USO_DEBUG_LIB==1)
#if (TIPO_DEVICE==0)
		printf_DEBUG("DEBUG gilson_encode_dl_add::: ig:%u, ERRO:%i, modo:%u, tipo1:%u, tipo2:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u\n", ig, erro, s_gil[ig].modo, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step);
#else  // PC
		printf("DEBUG gilson_encode_dl_add::: ig:%u, ERRO:%i, modo:%u, tipo1:%u, tipo2:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u\n", ig, erro, s_gil[ig].modo, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step);
#endif  // #if (TIPO_DEVICE==1)
#endif  // #if (USO_DEBUG_LIB==1)

	}

	return erro;
}

// const uint8_t item, const uint8_t tipo1, const uint8_t tipo2, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step
int32_t gilson_encode_dl_data(const uint8_t item, const uint8_t tipo1, const uint8_t tipo2, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	int32_t erro=erGSON_OK;

	erro = valid_gilson_encode_dl(item, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step);
	if(erro != erGSON_OK)
	{
		goto deu_erro;
	}

	// os 2 primeiro: 'chave', 'nome_chave' nao usamos aquiii vai ser ignorados devido 's_gil[ig].tipo_dinamico'
	erro = gilson_encode_data_base(0, "x", tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step);

	deu_erro:

	if(erro==erGSON_OK)
	{
		s_gil[ig].cont_tipo_dinamico += 1;
	}
	else
	{
		s_gil[ig].erro = erro;

#if (USO_DEBUG_LIB==1)
#if (TIPO_DEVICE==0)
		printf_DEBUG("DEBUG gilson_encode_dl_data::: ig:%u, ERRO:%i, modo:%u, tipo1:%u, tipo2:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u\n", ig, erro, s_gil[ig].modo, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step);
#else  // PC
		printf("DEBUG gilson_encode_dl_data::: ig:%u, ERRO:%i, modo:%u, tipo1:%u, tipo2:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u\n", ig, erro, s_gil[ig].modo, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step);
#endif  // #if (TIPO_DEVICE==1)
#endif  // #if (USO_DEBUG_LIB==1)

	}

	return erro;
}

int32_t gilson_encode_dl_end(void)
{
	int32_t erro=erGSON_OK;


	// se nao bateu o numero de 'tam_list' e/ou 'nitens' da pra gerar um erro...
	if(s_gil[ig].tipo_dinamico == 0)
	{
		erro = erGSON_48;
	}
	else if(s_gil[ig].tipo_operacao != e_OPER_ENCODE)
	{
		erro = erGSON_OPER;
	}
	else if(s_gil[ig].cont_tipo_dinamico != s_gil[ig].tam_list*s_gil[ig].nitens)
	{
		erro = erGSON_49;
	}


#if (USO_DEBUG_LIB==1)
#if (TIPO_DEVICE==0)
	printf_DEBUG("DEBUG gilson_encode_dl_end::: ig:%u, ERRO:%i, modo:%u, tam_list:%u, nitens:%u, tipo_dinamico:%u\n", ig, erro, s_gil[ig].modo, s_gil[ig].tam_list, s_gil[ig].nitens, s_gil[ig].cont_tipo_dinamico);
#else  // PC
	printf("DEBUG gilson_encode_dl_end::: ig:%u, ERRO:%i, modo:%u, tam_list:%u, nitens:%u, tipo_dinamico:%u\n", ig, erro, s_gil[ig].modo, s_gil[ig].tam_list, s_gil[ig].nitens, s_gil[ig].cont_tipo_dinamico);
#endif  // #if (TIPO_DEVICE==1)
#endif  // #if (USO_DEBUG_LIB==1)

	s_gil[ig].tipo_dinamico = 0;  // finalizando o tratar um tipo dinamico

	s_gil[ig].erro = erro;

	return erro;
}
//==========================================================================================================================================


// perigo de explodir o sistema, caso passe parâmetros errados ou esqueça de passar algum
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
			cont_list_a = (uint16_t)va_arg(argptr, int);
		}
	}
	else if(tipo1 == GSON_LIST)
	{
		cont_list_a = (uint16_t)va_arg(argptr, int);
		if(tipo2 == GSON_tSTRING)
		{
			cont_list_b = (uint16_t)va_arg(argptr, int);
		}
	}
	else if(tipo1 == GSON_MTX2D)
	{
		cont_list_a = (uint16_t)va_arg(argptr, int);
		cont_list_b = (uint16_t)va_arg(argptr, int);
		cont_list_step = (uint16_t)va_arg(argptr, int);
	}
	else
	{
		// erro
	}

	va_end(argptr);

	//printf("valor:%u\n", &valor);
	//printf("gilson_encode::: chave:%u, tipo1:%u, tipo2:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u\n", chave, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step);


	return gilson_encode_data_base(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step);
}



// entra com 'mapa' fixo do dados que serão encodados
// em '...' teremos 'nome_chave' caso seja KV e/ou 'valor' que é a data específica
// perigo de explodir o sistema, caso passe parâmetros errados ou esqueça de passar algum
int32_t gilson_encode_mapfix(const uint16_t *map, ...)
{
	uint16_t cont_list_a=0, cont_list_b=0, cont_list_step=0;
	uint8_t *valor;
	//uint8_t chave=0, tipo1=255, tipo2=255;
	char *nome_chave = {"\0"};
	va_list argptr;

	/*
	'map' pode ter até 6 'uint16_t'
	chave = map[0] (obrigatório)
	tipo1 = map[1] (obrigatório)
	tipo2 = map[2] (obrigatório)
	cont_list_a = map[3] (se for o caso...)
	cont_list_b = map[4] (se for o caso...)
	cont_list_step = map[5] (se for o caso...)
	*/

	va_start(argptr, map);

	if(s_gil[ig].modo == GSON_MODO_KV || s_gil[ig].modo == GSON_MODO_KV_ZIP)
	{
		nome_chave = va_arg(argptr, char *);
	}

	valor = va_arg(argptr, uint8_t *);

	if(map[1] == GSON_SINGLE)
	{
		if(map[2] == GSON_tSTRING)
		{
			cont_list_a = map[3];
		}
	}
	else if(map[1] == GSON_LIST)
	{
		cont_list_a = map[3];
		if(map[2] == GSON_tSTRING)
		{
			cont_list_b = map[4];
		}
	}
	else if(map[1] == GSON_MTX2D)
	{
		cont_list_a = map[3];
		cont_list_b = map[4];
		cont_list_step = map[5];
	}
	else
	{
		// erro
	}

	//printf("valor:%u\n", &valor);

	//printf("cont_list_a:%u, cont_list_b:%u, cont_list_step:%u\n", cont_list_a, cont_list_b, cont_list_step);

	va_end(argptr);


	return gilson_encode_data_base(map[0], nome_chave, map[1], map[2], valor, cont_list_a, cont_list_b, cont_list_step);
}


// entra com 'mapa' dinâmico dos dados que serão encodados
// em '...' teremos 'nome_chave' caso seja KV e/ou 'valor' que é a data específica, seguido de até 3 valores
// perigo de explodir o sistema, caso passe parâmetros errados ou esqueça de passar algum
int32_t gilson_encode_mapdin(const uint16_t *map, ...)
{
	uint16_t cont_list_a=0, cont_list_b=0, cont_list_step=0;
	uint8_t *valor;
	char *nome_chave = {"\0"};
	va_list argptr;

	/*
	'map' deve ter 3 'uint16_t'
	chave = map[0] (obrigatório)
	tipo1 = map[1] (obrigatório)
	tipo2 = map[2] (obrigatório)
	*/

	va_start(argptr, map);

	if(s_gil[ig].modo == GSON_MODO_KV || s_gil[ig].modo == GSON_MODO_KV_ZIP)
	{
		nome_chave = va_arg(argptr, char *);
	}

	valor = va_arg(argptr, uint8_t *);

	if(map[1] == GSON_SINGLE)
	{
		if(map[2] == GSON_tSTRING)
		{
			cont_list_a = (uint16_t)va_arg(argptr, int);
		}
	}
	else if(map[1] == GSON_LIST)
	{
		cont_list_a = (uint16_t)va_arg(argptr, int);
		if(map[2] == GSON_tSTRING)
		{
			cont_list_b = (uint16_t)va_arg(argptr, int);
		}
	}
	else if(map[1] == GSON_MTX2D)
	{
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


	return gilson_encode_data_base(map[0], nome_chave, map[1], map[2], valor, cont_list_a, cont_list_b, cont_list_step);
}


// quando temos um mapa de chaves definido e nao vamos encodadar uma ou mais chaves desse mapa devemos setar a chave como nula para nao bugar a logica da contagem das chaves
// perigo de explodir o sistema, caso passe parâmetros errados ou esqueça de passar algum
// só funciona no modos FULL
int32_t gilson_encode_data_null(const uint8_t chave, ...)
{
    char *nome_chave = {"\0"};
    uint8_t dummy=0;
    va_list argptr;

    /*
	if(s_gil[ig].modo == GSON_MODO_ZIP || s_gil[ig].modo == GSON_MODO_KV_ZIP)
	{
		// esquema de nulo nao aceita em modo ZIP!!
		if(s_gil[ig].erro == 0)
		return erGSON_NULL;
	}
	*/

    va_start(argptr, chave);

    //if(s_gil[ig].modo == GSON_MODO_KV || s_gil[ig].modo == GSON_MODO_KV_ZIP)
	if(s_gil[ig].modo == GSON_MODO_KV)
    {
        nome_chave = va_arg(argptr, char *);
    }

	va_end(argptr);

    return gilson_encode_data_base(chave, nome_chave, 255, 255, &dummy, 0, 0, 0);
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

	if(pack[0] == GSON_MODO_JSON)  // modo
	{
		erro = erGSON_dJSON;
		goto deu_erro;
	}

	s_gil[ig].tipo_operacao = e_OPER_DECODE;
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
		s_gil[ig].cont_itens2 = s_gil[ig].bufr[1];

		s_gil[ig].pos_bytes = OFFSET_MODO_ZIP;  // offset geral
	}
	else
	{
		memcpy(&crc1, &s_gil[ig].bufr[1], 4);
		memcpy(&s_gil[ig].pos_bytes2, &s_gil[ig].bufr[5], 2);
		s_gil[ig].cont_itens2 = s_gil[ig].bufr[7];

		crc2 = gil_crc(0xffffffff, &s_gil[ig].bufr[5], s_gil[ig].pos_bytes2-5);

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
		s_gil[ig].tipo_operacao = e_OPER_NULL;
	}


#if (USO_DEBUG_LIB==1)
#if (TIPO_DEVICE==0)
	printf_DEBUG("DEBUG gilson_decode_init::: ig:%u, erro:%i, end_ram:%X, modo:%u, pos_bytes:%u, cont_itens:%u, cru:[%u,%u,%u,%u,%u,%u,%u,%u], crc:%u==%u, pos_bytes2:%u, cont_itens2:%u\n", ig, erro, s_gil[ig].end_ram, s_gil[ig].modo, s_gil[ig].pos_bytes, s_gil[ig].cont_itens, s_gil[ig].bufr[0], s_gil[ig].bufr[1], s_gil[ig].bufr[2], s_gil[ig].bufr[3], s_gil[ig].bufr[4], s_gil[ig].bufr[5], s_gil[ig].bufr[6], s_gil[ig].bufr[7], crc1, crc2, s_gil[ig].pos_bytes2, s_gil[ig].cont_itens2);
#else  // PC
	printf("DEBUG gilson_decode_init::: ig:%u, erro:%i, end_ram:%X, modo:%u, pos_bytes:%u, cont_itens:%u, cru:[%u,%u,%u,%u,%u,%u,%u,%u], crc:%u==%u, pos_bytes2:%u, cont_itens2:%u\n", ig, erro, s_gil[ig].end_ram, s_gil[ig].modo, s_gil[ig].pos_bytes, s_gil[ig].cont_itens, s_gil[ig].bufr[0], s_gil[ig].bufr[1], s_gil[ig].bufr[2], s_gil[ig].bufr[3], s_gil[ig].bufr[4], s_gil[ig].bufr[5], s_gil[ig].bufr[6], s_gil[ig].bufr[7], crc1, crc2, s_gil[ig].pos_bytes2, s_gil[ig].cont_itens2);
#endif  // #if (TIPO_DEVICE==1)
#endif  // #if (USO_DEBUG_LIB==1)


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


static int32_t gilson_decode_end_base(const uint8_t flag_crc, uint32_t *crc)
{
	int32_t erro=erGSON_OK;

	if(s_gil[ig].erro != erGSON_OK)
	{
		erro = s_gil[ig].erro;  // vamos manter sempre o mesmo erro!!!
		goto deu_erro;
	}

	if(s_gil[ig].modo == GSON_MODO_ZIP || s_gil[ig].modo == GSON_MODO_KV_ZIP)
	{
		if(flag_crc)
		{
			s_gil[ig].crc_out = gil_crc(0xffffffff, s_gil[ig].bufr, s_gil[ig].pos_bytes);
		}
		else
		{
			s_gil[ig].crc_out = 0;
		}
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
			// indica que nao leu todas chaves ou nao foi até a última ou nao veu em ordem até o fim, mas está tudo bem
			s_gil[ig].pos_bytes = s_gil[ig].pos_bytes2;
			s_gil[ig].cont_itens = s_gil[ig].cont_itens2;
		}
	}

	s_gil[ig].ativo = 0;
	s_gil[ig].tipo_operacao = e_OPER_NULL;

	*crc = s_gil[ig].crc_out;

	deu_erro:

#if (USO_DEBUG_LIB==1)
#if (TIPO_DEVICE==0)
	printf_DEBUG("DEBUG gilson_decode_end_crc::: ig:%u, erro:%i, modo:%u, pos_bytes:%u==%u, cont_itens:%u==%u, crc:%u \n", ig, erro, s_gil[ig].modo, s_gil[ig].pos_bytes, s_gil[ig].pos_bytes2, s_gil[ig].cont_itens, s_gil[ig].cont_itens2, s_gil[ig].crc_out);
#else  // PC
	printf("DEBUG gilson_decode_end_crc::: ig:%u, erro:%i, modo:%u, pos_bytes:%u==%u, cont_itens:%u==%u, crc:%u \n", ig, erro, s_gil[ig].modo, s_gil[ig].pos_bytes, s_gil[ig].pos_bytes2, s_gil[ig].cont_itens, s_gil[ig].cont_itens2, s_gil[ig].crc_out);
#endif  // #if (TIPO_DEVICE==1)
#endif  // #if (USO_DEBUG_LIB==1)


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


int32_t gilson_decode_end(void)
{
	uint32_t crc=0;
	return gilson_decode_end_base(0, &crc);
}

int32_t gilson_decode_end_crc(uint32_t *crc)
{
	return gilson_decode_end_base(1, crc);
}


// colocar sempre o "(uint8_t *)" na frente da variavel de entrada 'multi_data' e passar como "&"
static int32_t gilson_decode_data_base(const uint8_t chave, char *nome_chave, const uint8_t tipo1, const uint8_t tipo2, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step, const uint8_t test_valor)
{
	int32_t erro=erGSON_OK;
	uint16_t i, vezes = 1, nbytes=1, temp16, len=0;

	uint16_t cont_list_aX=0, cont_list_bX=0, cont_list_stepX=0, pos_bytes_bk=0;
	uint8_t tipo_muxX=0, tipo1X=0, tipo2X=0;

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

	if(s_gil[ig].tipo_operacao!=e_OPER_DECODE)
	{
		erro = erGSON_OPER;
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

	if(chave >= s_gil[ig].cont_itens2)  // 's_gil[ig].tipo_dinamico==0' ????  && s_gil[ig].tipo_dinamico==0
	{
		// quer ler uma chave maior do que a total programado
		erro = erGSON_23b;
		goto deu_erro;
	}

	if(s_gil[ig].cont_itens>0 && chave <= s_gil[ig].chave_atual && s_gil[ig].tipo_dinamico==0)
	{
		erro = erGSON_SMKEYd;
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
		// 0baaabbbbb = a:tipo1, b=tipo2
		//bufr[pos_bytes] = tipo_mux;
		tipo_muxX = s_gil[ig].bufr[s_gil[ig].pos_bytes];
		tipo1X = s_gil[ig].bufr[s_gil[ig].pos_bytes];
		tipo2X = s_gil[ig].bufr[s_gil[ig].pos_bytes];
		tipo1X >>= 5;
		tipo2X &= 0b11111;
		s_gil[ig].pos_bytes += 1;
		pos_bytes_bk = s_gil[ig].pos_bytes;

		//printf("sacuuuuuuuuuuuuuuuuuuuuuuuu0 chave:%u, tipomuxX:%u, tipo1X:%u, tipo2X:%u, pos_bytes:%u\n", chave, tipomuxX, tipo1X, tipo2X, s_gil[ig].pos_bytes);
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
			memcpy(&cont_list_aX, &s_gil[ig].bufr[s_gil[ig].pos_bytes], 2);
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
					cont_list_bX = s_gil[ig].bufr[s_gil[ig].pos_bytes];
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
			cont_list_aX = s_gil[ig].bufr[s_gil[ig].pos_bytes];
			s_gil[ig].pos_bytes += 1;
			//memcpy(&bufr[pos_bytes], &cont_list_b, 2);
			memcpy(&cont_list_bX, &s_gil[ig].bufr[s_gil[ig].pos_bytes], 2);
			s_gil[ig].pos_bytes += 2;
			//memcpy(&bufr[pos_bytes], &cont_list_step, 2);
			memcpy(&cont_list_stepX, &s_gil[ig].bufr[s_gil[ig].pos_bytes], 2);
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

	//printf("gilson_decode_data_base::: modo:%u, chave:%u, cont_itens:%u, cont_itens2:%u\n", s_gil[ig].modo, chave, s_gil[ig].cont_itens, s_gil[ig].cont_itens2);

	// faz validacao do que tinha para com os parametros de entrada
	if((s_gil[ig].modo == GSON_MODO_FULL || s_gil[ig].modo == GSON_MODO_KV) && test_valor==1)
	{
		//printf("sacuuuuuuuuuuuuuuuuuuuuuuuu1 chave:%u, modo:%u, tipomuxX:%u, tipo1:%u|%u, tipo2:%u|%u, pos_bytes:%u, cont_list_a:%u|%u, cont_list_b:%u|%u, cont_list_step:%u|%u, vezes:%u\n", chave, s_gil[ig].modo, tipo_muxX, tipo1, tipo1X, tipo2, tipo2X, pos_bytes_bk, cont_list_a, cont_list_aX, cont_list_b, cont_list_b, cont_list_step, cont_list_step, vezes);

		if(tipo_muxX!=TIPO_GSON_NULL)
		{
			if(tipo1 != tipo1X || tipo2 != tipo2X || (tipo1!=GSON_SINGLE && cont_list_a < cont_list_aX) || cont_list_b < cont_list_bX || cont_list_step < cont_list_stepX)
			{
				//printf("sacuuuuuuuuuuuuuuuuuuuuuuuu1 chave:%u, modo:%u, tipomuxX:%u, tipo1:%u|%u, tipo2:%u|%u, pos_bytes:%u, cont_list_a:%u|%u, cont_list_b:%u|%u, cont_list_step:%u|%u, vezes:%u\n", chave, s_gil[ig].modo, tipomuxX, tipo1, tipo1X, tipo2, tipo2X, pos_bytes_bk, cont_list_a, cont_list_aX, cont_list_b, cont_list_b, cont_list_step, cont_list_step, vezes);
				erro = erGSON_DIFIN;
				goto deu_erro;
			}
		}

		// na realidade quem manda é o que está no pacote e nao o externo, logo é os "X"
		if(tipo1!=GSON_SINGLE && cont_list_a!=cont_list_aX)
		{
			vezes = cont_list_aX;
		}
	}
	else
	{
		tipo1X = tipo1;
		tipo2X = tipo2;
		cont_list_aX = cont_list_a;
		cont_list_bX = cont_list_b;
		cont_list_stepX = cont_list_step;
	}

	if(tipo_muxX!=TIPO_GSON_NULL)
	{
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
				if(test_valor==0)
				{
					memcpy(valor+(i*cont_list_bX), &s_gil[ig].bufr[s_gil[ig].pos_bytes], len);
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
				for(i=0; i<cont_list_aX; i++)
				{
					temp16 = i*cont_list_stepX*nbytes;  // passos em cada linha da matriz
					if(test_valor==0)
					{
						memcpy(valor+temp16, &s_gil[ig].bufr[s_gil[ig].pos_bytes], nbytes*cont_list_bX);
					}
					s_gil[ig].pos_bytes += nbytes*cont_list_bX;
				}
			}
			else
			{
				if(test_valor==0)
				{
					memcpy(valor, &s_gil[ig].bufr[s_gil[ig].pos_bytes], nbytes*vezes);
				}
				s_gil[ig].pos_bytes += nbytes*vezes;
			}
		}
	}
	else
	{
		s_gil[ig].chaves_null+=1;  // temos chaves nulas, vai incrementando para análise final
	}

	deu_erro:

	if(erro==erGSON_OK)
	{
		s_gil[ig].cont_itens += 1;

		s_gil[ig].chave_atual = chave;
	}
	else
	{
		s_gil[ig].erro = erro;

#if (USO_DEBUG_LIB==1)
#if (TIPO_DEVICE==0)
		printf_DEBUG("DEBUG gilson_decode_data::: ig:%u, ERRO:%i, modo:%u, chave:%u, tipo1:%u, tipo2:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u\n", ig, erro, s_gil[ig].modo, chave, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step);
#else  // PC
		printf("DEBUG gilson_decode_data::: ig:%u, ERRO:%i, chave:%u, modo:%u, tipo1:%u, tipo2:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u\n", ig, erro, s_gil[ig].modo, chave, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step);
#endif  // #if (TIPO_DEVICE==1)
#endif  // #if (USO_DEBUG_LIB==1)

	}

	return erro;
}


int32_t gilson_decode_data(const uint8_t chave, const uint8_t tipo1, const uint8_t tipo2, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy

	//printf("gilson_decode_data::: chave:%u, tipo1:%u, tipo2:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u\n", chave, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step);

	return gilson_decode_data_base(chave, temp, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step, 0);
}

int32_t gilson_decode_dataKV(const uint8_t chave, char *nome_chave, const uint8_t tipo1, const uint8_t tipo2, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	return gilson_decode_data_base(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step, 0);
}



// funcoes auxiliares fortemente tipadas de decode

//========================================= GSON_SINGLE
int32_t gilson_decode_u8(const uint8_t chave, uint8_t *valor)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_SINGLE, GSON_tUINT8, (uint8_t *)valor, 0, 0, 0, 0);
}

int32_t gilson_decode_s8(const uint8_t chave, int8_t *valor)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_SINGLE, GSON_tINT8, (uint8_t *)valor, 0, 0, 0, 0);
}

int32_t gilson_decode_u16(const uint8_t chave, uint16_t *valor)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_SINGLE, GSON_tUINT16, (uint8_t *)valor, 0, 0, 0, 0);
}

int32_t gilson_decode_s16(const uint8_t chave, int16_t *valor)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_SINGLE, GSON_tINT16, (uint8_t *)valor, 0, 0, 0, 0);
}

int32_t gilson_decode_u32(const uint8_t chave, uint32_t *valor)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_SINGLE, GSON_tUINT32, (uint8_t *)valor, 0, 0, 0, 0);
}

int32_t gilson_decode_s32(const uint8_t chave, int32_t *valor)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_SINGLE, GSON_tINT32, (uint8_t *)valor, 0, 0, 0, 0);
}


int32_t gilson_decode_u64(const uint8_t chave, uint64_t *valor)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_SINGLE, GSON_tUINT64, (uint8_t *)valor, 0, 0, 0, 0);
}


int32_t gilson_decode_s64(const uint8_t chave, int64_t *valor)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_SINGLE, GSON_tINT64, (uint8_t *)valor, 0, 0, 0, 0);
}


int32_t gilson_decode_f32(const uint8_t chave, float *valor)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_SINGLE, GSON_tFLOAT32, (uint8_t *)valor, 0, 0, 0, 0);
}


int32_t gilson_decode_f64(const uint8_t chave, double *valor)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_SINGLE, GSON_tFLOAT64, (uint8_t *)valor, 0, 0, 0, 0);
}


int32_t gilson_decode_str(const uint8_t chave, char *valor)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_SINGLE, GSON_tSTRING, (uint8_t *)valor, 0, 0, 0, 0);
}



//========================================= GSON_LIST
int32_t gilson_decode_lu8(const uint8_t chave, uint8_t valor[], const uint16_t cont_list_a)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_LIST, GSON_tUINT8, (uint8_t *)valor, cont_list_a, 0, 0, 0);
}


int32_t gilson_decode_ls8(const uint8_t chave, int8_t valor[], const uint16_t cont_list_a)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_LIST, GSON_tINT8, (uint8_t *)valor, cont_list_a, 0, 0, 0);
}


int32_t gilson_decode_lu16(const uint8_t chave, uint16_t valor[], const uint16_t cont_list_a)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_LIST, GSON_tUINT16, (uint8_t *)valor, cont_list_a, 0, 0, 0);
}


int32_t gilson_decode_ls16(const uint8_t chave, int16_t valor[], const uint16_t cont_list_a)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_LIST, GSON_tINT16, (uint8_t *)valor, cont_list_a, 0, 0, 0);
}


int32_t gilson_decode_lu32(const uint8_t chave, uint32_t valor[], const uint16_t cont_list_a)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_LIST, GSON_tUINT32, (uint8_t *)valor, cont_list_a, 0, 0, 0);
}


int32_t gilson_decode_ls32(const uint8_t chave, int32_t valor[], const uint16_t cont_list_a)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_LIST, GSON_tINT32, (uint8_t *)valor, cont_list_a, 0, 0, 0);
}


int32_t gilson_decode_lu64(const uint8_t chave, uint64_t valor[], const uint16_t cont_list_a)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_LIST, GSON_tUINT64, (uint8_t *)valor, cont_list_a, 0, 0, 0);
}


int32_t gilson_decode_ls64(const uint8_t chave, int64_t valor[], const uint16_t cont_list_a)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_LIST, GSON_tINT64, (uint8_t *)valor, cont_list_a, 0, 0, 0);
}


int32_t gilson_decode_lf32(const uint8_t chave, float valor[], const uint16_t cont_list_a)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_LIST, GSON_tFLOAT32, (uint8_t *)valor, cont_list_a, 0, 0, 0);
}


int32_t gilson_decode_lf64(const uint8_t chave, double valor[], const uint16_t cont_list_a)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_LIST, GSON_tFLOAT64, (uint8_t *)valor, cont_list_a, 0, 0, 0);
}


int32_t gilson_decode_lstr(const uint8_t chave, char *valor, const uint16_t cont_list_a, const uint16_t cont_list_b)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_LIST, GSON_tSTRING, (uint8_t *)valor, cont_list_a, cont_list_b, 0, 0);
}



//========================================= GSON_MTX2D
int32_t gilson_decode_mu8(const uint8_t chave, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_MTX2D, GSON_tUINT8, (uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step, 0);
}


int32_t gilson_decode_ms8(const uint8_t chave, int8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_MTX2D, GSON_tINT8, (uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step, 0);
}


int32_t gilson_decode_mu16(const uint8_t chave, uint16_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_MTX2D, GSON_tUINT16, (uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step, 0);
}


int32_t gilson_decode_ms16(const uint8_t chave, int16_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_MTX2D, GSON_tINT16, (uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step, 0);
}


int32_t gilson_decode_mu32(const uint8_t chave, uint32_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_MTX2D, GSON_tUINT32, (uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step, 0);
}


int32_t gilson_decode_ms32(const uint8_t chave, int32_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_MTX2D, GSON_tINT32, (uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step, 0);
}


int32_t gilson_decode_mu64(const uint8_t chave, uint64_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_MTX2D, GSON_tUINT64, (uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step, 0);
}


int32_t gilson_decode_ms64(const uint8_t chave, int64_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_MTX2D, GSON_tINT64, (uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step, 0);
}


int32_t gilson_decode_mf32(const uint8_t chave, float *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_MTX2D, GSON_tFLOAT32, (uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step, 0);
}


int32_t gilson_decode_mf64(const uint8_t chave, double *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	char temp[LEN_MAX_CHAVE_NOME];  // dummy
	return gilson_decode_data_base(chave, temp, GSON_MTX2D, GSON_tFLOAT64, (uint8_t *)valor, cont_list_a, cont_list_b, cont_list_step, 0);
}







// só funciona para modo 'GSON_MODO_FULL'!!!!
static int32_t gilson_decode_data_full_base(const uint8_t chave, char *nome_chave, uint8_t *valor)
{
	int32_t erro=erGSON_OK;
	uint16_t i, vezes = 1, cont_list_a=0, cont_list_b=0, cont_list_step=0, nbytes=1, temp16, pos_bytes, len, pos_bytes_bk=0;
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

	if(s_gil[ig].tipo_operacao!=e_OPER_DECODE)
	{
		erro = erGSON_OPER;
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
		// quer ler uma chave maior do que a contagem total que é decodificada em decode_init e aloca em 'cont_itens2'
		// nesse caso nao tem como prosseguir!!!!
		erro = erGSON_30;
		goto deu_erro;
	}


#if (USO_DEBUG_LIB==1 && PRINT_DEBUG==1)
#if (TIPO_DEVICE==0)
	printf_DEBUG("DEBUG gilson_decode_data_full_base init::: ig:%u, chave:%u, nulos:%u, cont_itens:%u, cont_itens2:%u, pos_bytes:%u\n", ig, chave, s_gil[ig].chaves_null, s_gil[ig].cont_itens, s_gil[ig].cont_itens2, s_gil[ig].pos_bytes);
#else  // PC
	printf("DEBUG gilson_decode_data_full_base init::: ig:%u, chave:%u, nulos:%u, cont_itens:%u, cont_itens2:%u, pos_bytes:%u\n", ig, chave, s_gil[ig].chaves_null, s_gil[ig].cont_itens, s_gil[ig].cont_itens2, s_gil[ig].pos_bytes);
#endif  // #if (TIPO_DEVICE==1)
#endif  // #if (USO_DEBUG_LIB==1)


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


		if(s_gil[ig].tipo_dinamico == 0)
		{
			if(s_gil[ig].modo == GSON_MODO_KV)
			{
				// nome da chave
				len = s_gil[ig].bufr[s_gil[ig].pos_bytes];
				s_gil[ig].pos_bytes += 1;
				memcpy(nome_chave, &s_gil[ig].bufr[s_gil[ig].pos_bytes], len);
				s_gil[ig].pos_bytes += len;

				// len vai ser menor que 'LEN_MAX_CHAVE_NOME' caracteres...
			}

			// nao estamos no modo dinamico lista
			pos_bytes = s_gil[ig].pos_bytes;
		}
		else
		{
			// estamos no modo dinamico lista
			pos_bytes = s_gil[ig].pos_bytes_dl;  // onde se encontra a parte do header e está no ponto onde depende do 'item' que foi setado em 'gilson_decode_dl()'
			// e lembrando que 's_gil[ig].pos_bytes' está ja em ponto da 'data' de fato!!! e na sequencia continua dos 'itens' chamados em ordem!!!!
			// OBS: até entao ele ja validou o header mas aqui abaixo vai fazer tudo novamente... mas vamos melhorar isso no futuro...
		}


		tipo_mux = s_gil[ig].bufr[pos_bytes];
		pos_bytes += 1;
		pos_bytes_bk = pos_bytes;


		if(tipo_mux != TIPO_GSON_NULL)
		{
			// 0baaabbbbb = a:tipo1, b=tipo2
			//tipo_mux = tipo1<<5;
			//tipo_mux |= tipo2;
			tipo1 = tipo_mux>>5;
			tipo2 = tipo_mux&0b11111;
			//printf("decode: 0baaabbbbb = a:tipo1(%u), b=tipo2()%u tipo_mux:%u\n", tipo1, tipo2, tipo_mux);

			if(tipo1>=GSON_MAX)
			{
				erro = erGSON_31;
				goto deu_erro;
			}
			if(tipo2>=GSON_tMAX)
			{
				erro = erGSON_31b;
				goto deu_erro;
			}

			if(tipo1 == GSON_LIST)
			{
				memcpy(&vezes, &s_gil[ig].bufr[pos_bytes], 2);
				pos_bytes += 2;

				if(vezes == 0)
				{
					erro = erGSON_32;
					goto deu_erro;
				}

				if(tipo2 == GSON_tSTRING)
				{
					cont_list_b = s_gil[ig].bufr[pos_bytes];
					pos_bytes += 1;

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
				cont_list_a = s_gil[ig].bufr[pos_bytes];
				pos_bytes += 1;
				memcpy(&cont_list_b, &s_gil[ig].bufr[pos_bytes], 2);
				pos_bytes += 2;
				memcpy(&cont_list_step, &s_gil[ig].bufr[pos_bytes], 2);
				pos_bytes += 2;

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


			// terminada os decodes de tipos...
			if(s_gil[ig].tipo_dinamico == 0)
			{
				s_gil[ig].pos_bytes = pos_bytes;  // segue o baile de onde estava...
			}
			else
			{
				s_gil[ig].pos_bytes_dl = pos_bytes;  // para a próxima vez...
				// lembrando que 's_gil[ig].pos_bytes' ja está na posição correta da sequência
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
		}
		else
		{
			if(bypass == 0)
			{
				s_gil[ig].chaves_null+=1;  // temos chaves nulas, vai incrementando para análise final
			}

			// terminada os decodes de tipos...
			if(s_gil[ig].tipo_dinamico == 0)
			{
				s_gil[ig].pos_bytes = pos_bytes;  // segue o baile de onde estava...
			}
			else
			{
				s_gil[ig].pos_bytes_dl = pos_bytes;  // para a próxima vez...
				// lembrando que 's_gil[ig].pos_bytes' ja está na posição correta da sequência
			}

		}
		// else (tipo1==255 && tipo2==255) temos um caso NULL de chave!!!


		//printf("sacuuuuuuuuuuuuuuuuuuuuuuuu2 chave:%u, tipomuxX:%u, tipo1X:%u, tipo2X:%u, pos_bytes:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u, vezes:%u\n", chave, tipo_mux, tipo1, tipo2, pos_bytes_bk, cont_list_a, cont_list_b, cont_list_step, vezes);

#if (USO_DEBUG_LIB==1 && PRINT_DEBUG==1)
#if (TIPO_DEVICE==0)
		printf_DEBUG("DEBUG gilson_decode_data_full_base loop::: ig:%u, ERRO:%i, chave:%u, bypass:%u, tipo1:%u, tipo2:%u, nulos:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u, cont_itens:%u, pos_bytes:%u\n", ig, erro, chave, bypass, tipo1, tipo2, s_gil[ig].chaves_null, cont_list_a, cont_list_b, cont_list_step, s_gil[ig].cont_itens, s_gil[ig].pos_bytes);
#else  // PC
		printf("DEBUG gilson_decode_data_full_base loop::: ig:%u, ERRO:%i, chave:%u, bypass:%u, tipo1:%u, tipo2:%u, nulos:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u, cont_itens:%u, pos_bytes:%u\n", ig, erro, chave, bypass, tipo1, tipo2, s_gil[ig].chaves_null, cont_list_a, cont_list_b, cont_list_step, s_gil[ig].cont_itens, s_gil[ig].pos_bytes);
#endif  // #if (TIPO_DEVICE==1)
#endif  // #if (USO_DEBUG_LIB==1)


		s_gil[ig].erro = erro;

		deu_erro:

		if(erro==erGSON_OK)
		{
			s_gil[ig].cont_itens += 1;
		}
		else
		{
			break;  // deu erro, cai fora...
		}

		if(bypass == 0)
		{
			break;
		}
	}  // fim while(1)


#if (USO_DEBUG_LIB==1 && PRINT_DEBUG==1)
#if (TIPO_DEVICE==0)
	printf_DEBUG("DEBUG gilson_decode_data_full_base::: ig:%u, ERRO:%i, chave:%u, bypass:%u, tipo1:%u, tipo2:%u, nulos:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u, cont_itens:%u, pos_bytes:%u\n", ig, erro, chave, bypass, tipo1, tipo2, s_gil[ig].chaves_null, cont_list_a, cont_list_b, cont_list_step, s_gil[ig].cont_itens, s_gil[ig].pos_bytes);
#else  // PC
	printf("DEBUG gilson_decode_data_full_base::: ig:%u, ERRO:%i, chave:%u, bypass:%u, tipo1:%u, tipo2:%u, nulos:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u, cont_itens:%u, pos_bytes:%u\n", ig, erro, chave, bypass, tipo1, tipo2, s_gil[ig].chaves_null, cont_list_a, cont_list_b, cont_list_step, s_gil[ig].cont_itens, s_gil[ig].pos_bytes);
#endif  // #if (TIPO_DEVICE==1)
#endif  // #if (USO_DEBUG_LIB==1)

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


//==========================================================================================================================================
// só funciona nos modos FULL
int32_t gilson_decode_dl_init(const uint8_t chave)
{
	int32_t erro=erGSON_OK;
	uint16_t vezes = 1, cont_list_a=0, cont_list_b=0, cont_list_step=0;
	uint8_t i, tipo_mux=0, tipo1=255, tipo2=255;

	// OBS: nao foi testado ainda em modo KV

	if(s_gil[ig].erro != erGSON_OK)
	{
		erro = s_gil[ig].erro;  // vamos manter sempre o mesmo erro!!!
		goto deu_erro;
	}

	if(s_gil[ig].ativo==0)
	{
		erro = erGSON_50;
		goto deu_erro;
	}

	if(s_gil[ig].tipo_dinamico == 1)
	{
		// ja esta ativo esse modo... tem que teminar antes para iniciar um novo
		erro = erGSON_51;
		goto deu_erro;
	}

	if(chave > s_gil[ig].cont_itens2)
	{
		// quer add uma chave maior do que a contagem crescente... nao tem como
		erro = erGSON_52;
		goto deu_erro;
	}

	if(s_gil[ig].bufr[s_gil[ig].pos_bytes] == TIPO_GSON_LDIN)
	{
		s_gil[ig].tipo_operacao = e_OPER_DECODE;

		s_gil[ig].tipo_dinamico = 1;  // vamos tratar um tipo dinamico

		s_gil[ig].pos_bytes += 1;

		s_gil[ig].tam_list = s_gil[ig].bufr[s_gil[ig].pos_bytes];
		s_gil[ig].pos_bytes += 1;

		s_gil[ig].nitens = s_gil[ig].bufr[s_gil[ig].pos_bytes];
		s_gil[ig].pos_bytes += 1;

		s_gil[ig].pos_tipo_dl_init = s_gil[ig].pos_bytes;  // offset onde inicia os tipos de dados que vamos decodificar automaticamente
		s_gil[ig].cont_tipo_dinamico = 0;
		s_gil[ig].tam_list2 = 0;
		s_gil[ig].nitens2 = 0;
		s_gil[ig].chave_dl = chave;
	}
	else
	{
		erro = erGSON_53;
		goto deu_erro;
	}


	// vamos varrer nosso header para saber onde termina para entao saber onde começa os dados de fato
	for(i=0; i<s_gil[ig].nitens; i++)
	{
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
			erro = erGSON_54;
			goto deu_erro;
		}

		if(tipo1 == GSON_LIST)
		{
			memcpy(&vezes, &s_gil[ig].bufr[s_gil[ig].pos_bytes], 2);
			s_gil[ig].pos_bytes += 2;

			if(vezes == 0)
			{
				erro = erGSON_55;
				goto deu_erro;
			}

			if(tipo2 == GSON_tSTRING)
			{
				cont_list_b = s_gil[ig].bufr[s_gil[ig].pos_bytes];
				s_gil[ig].pos_bytes += 1;

				if(cont_list_b==0)
				{
					// precisamos do offset cru da lista de strings...
					erro = erGSON_56;
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
				erro = erGSON_57;
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
	}
	s_gil[ig].pos_tipo_dl_end = s_gil[ig].pos_bytes;  // onde começa a 'data' de fato!!!!!



	deu_erro:

	if(erro==erGSON_OK)
	{
		s_gil[ig].cont_itens += 1;
	}
	else
	{
		s_gil[ig].erro = erro;
	}


#if (USO_DEBUG_LIB==1)
#if (TIPO_DEVICE==0)
	printf_DEBUG("DEBUG gilson_decode_dl_init::: ig:%u, ERRO:%i, modo:%u, chave:%u, tam_list:%u, nitens:%u\n", ig, erro, s_gil[ig].modo, chave, s_gil[ig].tam_list, s_gil[ig].nitens);
#else  // PC
	printf("DEBUG gilson_decode_dl_init::: ig:%u, ERRO:%i, modo:%u, chave:%u, tam_list:%u, nitens:%u\n", ig, erro, s_gil[ig].modo, chave, s_gil[ig].tam_list, s_gil[ig].nitens);
#endif  // #if (TIPO_DEVICE==1)
#endif  // #if (USO_DEBUG_LIB==1)

	return erro;
}

/*
int32_t gilson_decode_dl_data_zip(const uint8_t item, const uint8_t tipo1, const uint8_t tipo2, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	int32_t erro=erGSON_OK;
	char temp[4];

	if(s_gil[ig].tipo_operacao!=e_OPER_DECODE)
	{
		erro = erGSON_OPER;
		goto deu_erro;
	}

	if(item > s_gil[ig].nitens)
	{
		// quer add uma item maior do que a contagem crescente... nao tem como
		erro = erGSON_59;
		goto deu_erro;
	}

	// com base no 'item' vamos deixar o offset pronto para em 'gilson_decode_data_full_base' saber como resgatar o 'tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step' do header
	if(item == 0)
	{
		s_gil[ig].pos_bytes_dl = s_gil[ig].pos_tipo_dl_init;  // posicao original do 'header'
	}

	erro = gilson_decode_data_base(s_gil[ig].chave_dl, temp, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step, 0);

	deu_erro:

	if(erro==erGSON_OK)
	{
		s_gil[ig].cont_tipo_dinamico += 1;
	}
	else
	{
		s_gil[ig].erro = erro;

#if (USO_DEBUG_LIB==1)
#if (TIPO_DEVICE==0)
		printf_DEBUG("DEBUG gilson_decode_dl::: ig:%u, ERRO:%i, modo:%u, item:%u\n", ig, erro, s_gil[ig].modo, item);
#else  // PC
		printf("DEBUG gilson_decode_dl::: ig:%u, ERRO:%i, modo:%u, item:%u\n", ig, erro, s_gil[ig].modo, item);
#endif  // #if (TIPO_DEVICE==1)
#endif  // #if (USO_DEBUG_LIB==1)

	}

	return erro;
}
*/

int32_t gilson_decode_dl_data(const uint8_t item, uint8_t *valor)
{
	int32_t erro=erGSON_OK;
	char temp[4];

	// somente funciona no modo 'GSON_MODO_FULL' e 'GSON_MODO_KV'!!!!
	if(s_gil[ig].modo != GSON_MODO_FULL && s_gil[ig].modo != GSON_MODO_KV)
	{
		erro = erGSON_58;
		goto deu_erro;
	}

	if(s_gil[ig].tipo_operacao!=e_OPER_DECODE)
	{
		erro = erGSON_OPER;
		goto deu_erro;
	}

	if(item > s_gil[ig].nitens)
	{
		// quer add uma item maior do que a contagem crescente... nao tem como
		erro = erGSON_59;
		goto deu_erro;
	}

	// com base no 'item' vamos deixar o offset pronto para em 'gilson_decode_data_full_base' saber como resgatar o 'tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step' do header
	if(item == 0)
	{
		s_gil[ig].pos_bytes_dl = s_gil[ig].pos_tipo_dl_init;  // posicao original do 'header'
	}

	erro = gilson_decode_data_full_base(s_gil[ig].chave_dl, temp, valor);

	deu_erro:

	if(erro==erGSON_OK)
	{
		s_gil[ig].cont_tipo_dinamico += 1;
	}
	else
	{
		s_gil[ig].erro = erro;

#if (USO_DEBUG_LIB==1)
#if (TIPO_DEVICE==0)
		printf_DEBUG("DEBUG gilson_decode_dl::: ig:%u, ERRO:%i, modo:%u, item:%u, chave_dl:%u\n", ig, erro, s_gil[ig].modo, item, s_gil[ig].chave_dl);
#else  // PC
		printf("DEBUG gilson_decode_dl::: ig:%u, ERRO:%i, modo:%u, item:%u, chave_dl:%u\n", ig, erro, s_gil[ig].modo, item, s_gil[ig].chave_dl);
#endif  // #if (TIPO_DEVICE==1)
#endif  // #if (USO_DEBUG_LIB==1)

	}

	return erro;
}



int32_t gilson_decode_dl_end(void)
{
	int32_t erro=erGSON_OK;

	// se nao bateu o numero de 'tam_list' e/ou 'nitens' da pra gerar um erro...
	if(s_gil[ig].tipo_dinamico == 0)
	{
		erro = erGSON_60;
	}
	else if(s_gil[ig].cont_tipo_dinamico != s_gil[ig].tam_list*s_gil[ig].nitens)
	{
		erro = erGSON_61;
	}


#if (USO_DEBUG_LIB==1)
#if (TIPO_DEVICE==0)
	printf_DEBUG("DEBUG gilson_decode_dl_end::: ig:%u, ERRO:%i, modo:%u, tam_list:%u, nitens:%u, tipo_dinamico:%u\n", ig, erro, s_gil[ig].modo, s_gil[ig].tam_list, s_gil[ig].nitens, s_gil[ig].cont_tipo_dinamico);
#else  // PC
	printf("DEBUG gilson_decode_dl_end::: ig:%u, ERRO:%i, modo:%u, tam_list:%u, nitens:%u, tipo_dinamico:%u\n", ig, erro, s_gil[ig].modo, s_gil[ig].tam_list, s_gil[ig].nitens, s_gil[ig].cont_tipo_dinamico);
#endif  // #if (TIPO_DEVICE==1)
#endif  // #if (USO_DEBUG_LIB==1)

	s_gil[ig].tipo_dinamico = 0;  // finalizando o tratar um tipo dinamico

	// OBS: finaliza somente esse tipo de dado 'lista dinâmica' mas a finalização do pacote geral é dado por 'gilson_decode_end_base()'

	return erro;
}
//==========================================================================================================================================




// perigo de explodir o sistema, caso passe parâmetros errados ou esqueça de passar algum
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
				cont_list_a = (uint16_t)va_arg(argptr, int);
			}
		}
		else if(tipo1 == GSON_LIST)
		{
			cont_list_a = (uint16_t)va_arg(argptr, int);
			if(tipo2 == GSON_tSTRING)
			{
				cont_list_b = (uint16_t)va_arg(argptr, int);
			}
		}
		else if(tipo1 == GSON_MTX2D)
		{
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

	//printf("valor:%u\n", &valor);
	//printf("gilson_decode::: chave:%u, tipo1:%u, tipo2:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u\n", chave, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step);


	if(modofull==0)
	{
		return gilson_decode_data_base(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step, 0);
	}
	else
	{
		return gilson_decode_data_full_base(chave, nome_chave, valor);
	}
}



// só funciona para modos FULL
int32_t gilson_decode_valid_map(const uint16_t map_full[][6], const uint16_t tot_chaves, const uint8_t *pack)
{
	int32_t erro=erGSON_OK, pos_bytes=0;
	uint8_t modo=0, i, dummy;
	char temp[LEN_MAX_CHAVE_NOME];  // dummy

	//printf("tamanho de map_full:%lu\n", sizeof(map_full));

	erro = gilson_decode_init(pack, &modo);

	if(modo == GSON_MODO_FULL || modo == GSON_MODO_KV)
	{
		if(tot_chaves == s_gil[ig].cont_itens2)
		{
			for(i=0; i<s_gil[ig].cont_itens2; i++)
			{
				// MODO ZIP: complica pois caso algum 'cont_list_a', 'cont_list_b' ou 'cont_list_step' seja dinâmico e não segue a ordem max do 'map_full[][]' ai não tem como dar certo essa rotina
				erro = gilson_decode_data_base(map_full[i][0], temp, map_full[i][1], map_full[i][2], CAST_GIL dummy, map_full[i][3], map_full[i][4], map_full[i][5], 1);  // vamos testar...
			}
			pos_bytes = gilson_decode_end();

			if(pos_bytes<=0)
			{
				erro=pos_bytes;
			}
		}
		else
		{
			// e se if(tot_chaves > LIMIT_GSON_KEYS)  erro = erGSON_LIMKEY;
			erro = erGSON_DIFKEY;
		}
	}
	else
	{
		erro =  erGSON_DIFKEYb;
	}

	memset(&s_gil[ig], 0x00, sizeof(s_gil[ig]));  // limpa para liberar pois nao vamos continuar o decode...

	return erro;
}



// entra com 'mapa' fixo dos dados que serão decodados
// em '...' teremos 'nome_chave' caso seja KV e/ou 'valor' que é a data específica
// perigo de explodir o sistema, caso passe parâmetros errados ou esqueça de passar algum
int32_t gilson_decode_mapfix(const uint16_t *map, ...)
{
	uint16_t cont_list_a=0, cont_list_b=0, cont_list_step=0;
	uint8_t chave=0, tipo1=255, tipo2=255, modofull=1;
	uint8_t *valor;
	char *nome_chave = {"\0"};
	va_list argptr;

	/*
	'map' pode ter até 6 'uint16_t'
	chave = map[0] (obrigatório)
	tipo1 = map[1] (obrigatório)
	tipo2 = map[2] (obrigatório)
	cont_list_a = map[3] (se for o caso...)
	cont_list_b = map[4] (se for o caso...)
	cont_list_step = map[5] (se for o caso...)
	*/

	va_start(argptr, map);

	chave = map[0];
	tipo1 = map[1];
	tipo2 = map[2];

	if(s_gil[ig].modo == GSON_MODO_KV || s_gil[ig].modo == GSON_MODO_KV_ZIP)
	{
		nome_chave = va_arg(argptr, char *);
	}

	if(s_gil[ig].modo != GSON_MODO_FULL && s_gil[ig].modo != GSON_MODO_KV)
	{
		modofull = 0;
	}

	valor = va_arg(argptr, uint8_t *);

	if(modofull==0)
	{
		if(tipo1 == GSON_SINGLE)
		{
			if(tipo2 == GSON_tSTRING)
			{
				cont_list_a = map[3];
			}
		}
		else if(tipo1 == GSON_LIST)
		{
			cont_list_a = map[3];
			if(tipo2 == GSON_tSTRING)
			{
				cont_list_b = map[4];
			}
		}
		else if(tipo1 == GSON_MTX2D)
		{
			cont_list_a = map[3];
			cont_list_b = map[4];
			cont_list_step = map[5];
		}
		else
		{
			// erro
		}
	}

	va_end(argptr);


	if(modofull==0)
	{
		return gilson_decode_data_base(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step, 0);
	}
	else
	{
		return gilson_decode_data_full_base(chave, nome_chave, valor);
	}
}


// entra com 'mapa' dinâmico dos dados que serão decodados
// em '...' teremos 'nome_chave' caso seja KV e/ou 'valor' que é a data específica, seguido de até 3 valores
// perigo de explodir o sistema, caso passe parâmetros errados ou esqueça de passar algum
int32_t gilson_decode_mapdin(const uint16_t *map, ...)
{
	uint16_t cont_list_a=0, cont_list_b=0, cont_list_step=0;
	uint8_t chave=0, tipo1=255, tipo2=255, modofull=1;
	uint8_t *valor;
	char *nome_chave = {"\0"};
	va_list argptr;

	/*
	'map' deve ter 3 'uint16_t'
	chave = map[0] (obrigatório)
	tipo1 = map[1] (obrigatório)
	tipo2 = map[2] (obrigatório)
	*/

	va_start(argptr, map);

	chave = map[0];
	tipo1 = map[1];
	tipo2 = map[2];

	if(s_gil[ig].modo == GSON_MODO_KV || s_gil[ig].modo == GSON_MODO_KV_ZIP)
	{
		nome_chave = va_arg(argptr, char *);
	}

	if(s_gil[ig].modo != GSON_MODO_FULL && s_gil[ig].modo != GSON_MODO_KV)
	{
		//tipo1 = (uint8_t)va_arg(argptr, int);  vai ter que bater caso seja FULL
		//tipo2 = (uint8_t)va_arg(argptr, int);  vai ter que bater caso seja FULL
		modofull = 0;
	}

	valor = va_arg(argptr, uint8_t *);

	if(modofull==0)
	{
		if(tipo1 == GSON_SINGLE)
		{
			if(tipo2 == GSON_tSTRING)
			{
				cont_list_a = (uint16_t)va_arg(argptr, int);
			}
		}
		else if(tipo1 == GSON_LIST)
		{
			cont_list_a = (uint16_t)va_arg(argptr, int);
			if(tipo2 == GSON_tSTRING)
			{
				cont_list_b = (uint16_t)va_arg(argptr, int);
			}
		}
		else if(tipo1 == GSON_MTX2D)
		{
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
		return gilson_decode_data_base(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step, 0);
	}
	else
	{
		return gilson_decode_data_full_base(chave, nome_chave, valor);
	}
}


// somente para 'GSON_MODO_FULL'
// buffer 'pack' deve suportar o pacote completo para alocar o resultado em 'valor'
int32_t gilson_decode_key(const uint8_t *pack, const uint8_t chave, uint8_t *valor)
{
	int32_t erro=erGSON_OK, pos_bytes=0;
	uint8_t modo=0;

	erro = gilson_decode_init(pack, &modo);
	if(erro==0 && modo==GSON_MODO_FULL)
	{
		erro = gilson_decode(chave, valor);
		pos_bytes = gilson_decode_end();
	}
	else
	{
		erro = erGSON_62;
	}


#if (USO_DEBUG_LIB==1)
#if (TIPO_DEVICE==0)
	printf_DEBUG("DEBUG gilson_decode_key::: erro:%i, modo:%u, chave:%u, pos_bytes:%i\n", erro, modo, chave, pos_bytes);
#else  // PC
	printf("DEBUG gilson_decode_key::: erro:%i, modo:%u, chave:%u, pos_bytes:%i\n", erro, modo, chave, pos_bytes);
#endif  // #if (TIPO_DEVICE==1)
#endif  // #if (USO_DEBUG_LIB==1)


	return erro;
}




//=================================================================================================================================================
//=================================================================================================================================================
//=================================================================================================================================================
//=================================================================================================================================================
// abaixo tudo em testes ainda....


static uint8_t compara_str(const char *v1, const char *v2)  // 'v2' sempre deve ser uma string pura que vai ser comparada
{
    uint8_t i, cont=0;
    uint8_t len = strlen(v2);  // strlen ignora o '\0' em uma string char.... logo cuidar isso, sizeof(v2) soma o '\0'
    for(i=0; i<len; i++)
    {
		 if(v1[i] == v2[i])
		 {
			cont+=1;
		 }
    }

    if(len==cont)
    {
        return 1;  // ok, nomes são iguais
    }
    else
    {
        return 0;  // não sao iguais
    }
    //return cont;
}

// verifica se a string 'sms' possui o caracter 'c' no tamanho de 'len' da string
// 1=tem, 0=nao tem
static int check_chr_str(const char *sms, const char c, const int len)
{
	int i, check=0;
	for(i=0; i<len; i++)
	{
		if(sms[i] == c)
		{
			check=1;
			break;
		}
	}
	return check;
}

// verifica se a string 'sms' possui o caracter 'c' no tamanho de 'len' da string
// >=1=total de vezes que o caractere apareceu, 0=nao tem
static int count_chr_str(const char *sms, const char c, const int len)
{
	int i, cont=0;
	for(i=0; i<len; i++)
	{
		if(sms[i] == c)
		{
			cont+=1;
		}
	}
	return cont;
}

// encontra o primeiro index do caracter 'c' na string 'sms' e retorna a posicao/index
static int pos_str_chr(const char *sms, const char c)
{
	char *ptr = strchr(sms, c);
	if (ptr != NULL)
	{
		return ptr - sms;
	}
	else
	{
		return -1;
	}
}

// encontra o primeiro index do caracter 'c' na string 'sms' e retorna a posicao/index
static int pos_str_str(const char *sms, const char *sms_alvo)
{
	char *ptr = strstr(sms, sms_alvo);
	if (ptr != NULL)
	{
		return ptr - sms;
	}
	else
	{
		return -1;
	}
}

static int count_str_str(const char *sms, const char *sms_alvo, const int len)
{
	int i=0, cont=0, pos, lena;
	lena = strlen(sms_alvo);
	//printf("sms:|%s|, alvo:|%s|, len:%u, lena:%u\n", sms, sms_alvo, len, lena);
	while(1)
	{
		pos = pos_str_str(&sms[i], sms_alvo);  // a primeira ocorrência...
		if(pos>=0)
		{
			//printf("pos:%u, cont:%u, i:%u, sms:|%s|\n", pos, cont, i, &sms[i]);
			i += (pos + lena);
			if(i<len)
			{
				cont+=1;
			}
		}

		i+=1;
		if(i>=len)
		{
			break;
		}
	}
	return cont;
}

/*
// by chétsss, meia bocaaa
static int string_flutuante_valida(const char *str)
{
    if (!str || !*str) return 0;

    char *endptr;
    double val = strtod(str, &endptr);

    if (endptr == str || isnan(val) || isinf(val))
        return 0;

    return 1;
}
*/

/*
#define LIMIT_U8		255
#define LIMIT_S8a		-128
#define LIMIT_S8b		127
#define LIMIT_U16		65535
#define LIMIT_S16a		-32768
#define LIMIT_S16b		32767
#define LIMIT_U32		4294967295
#define LIMIT_S32a		-2147483648
#define LIMIT_S32b		2147483647
#define LIMIT_U64		4294967295
#define LIMIT_S64a		-2147483648
#define LIMIT_S64b		2147483647
*/


static int info_numero(const char *str_num, int *flag_neg_ok_)
{
	// tem '.' nisso? se sim é um flutuante
	// quantos algarismos tem? até 255, 65535, 2**32-1, u64
	// é negativo? (-128 a 127), (-32.768 a 32.767), (-2.147.483.648 a 2.147.483.647) s64
	// float ou double? se total de casas decimais for >5~6 entao passa para double, ou entao se a parte inteira for >10e7 entao é double
	//double valor_flut = 0;
	//uint64_t valor_int=0;
	int tipo=0, pos, flag_neg_ok=0;
	uint8_t flag_neg=0, flag_flu=0, len=0, len_int=0, len_deci=0;

	len = strlen(str_num);
	if((uint8_t)str_num[0] == '-')
	{
		flag_neg = 1;  // temos um numero negativo
	}
	pos = pos_str_chr(str_num, '.');
	if(pos>0)  // investiga FLUTUANTE
	{
		flag_flu = 1;  // temos um flutuante
		len_int = pos;
		len_deci = len - len_int;
		if(flag_neg)
		{
			len_int-=1;
			len_deci-=1;
		}

		// f32 ou f64????
		if(len_deci>5)
		{
			tipo = GSON_tFLOAT64;
		}
		else
		{
			tipo = GSON_tFLOAT32;

			/*
			// analisar se está dentro dos limites MAX e MIN do FLOAT32
			// ????
			double valor_flut = atof(str_num);
			if(valor_flut > FLT_MAX || valor_flut < FLT_MIN)
			{
				tipo = GSON_tFLOAT64;
			}
			*/
		}

		flag_neg_ok=1;  // fica sempre em 1
	}
	else  // investiga INTEIRO
	{
		if(flag_neg)
		{
			char *endptr;
			int64_t valor_int = (int64_t)strtoll(str_num, &endptr, 10);

			if(valor_int>=INT8_MIN)
			{
				tipo = GSON_tINT8;  // s8
			}
			else if(valor_int>=INT16_MIN)
			{
				tipo = GSON_tINT16;  // s16
			}
			else if(valor_int>=INT32_MIN)  // -2147483648
			{
				tipo = GSON_tINT32;  // s32
			}
			else
			{
				tipo = GSON_tINT64;  // s64
			}
			flag_neg_ok=1;  // fica sempre em 1
		}
		else
		{
			// vamos lidar com o pior casoo
			//uint64_t valor_int = atoll(str_num);
			char *endptr;
			//errno = 0;  // Limpa possíveis erros anteriores
			uint64_t valor_int = (uint64_t)strtoull(str_num, &endptr, 10);

			if(valor_int<=UINT8_MAX)
			{
				tipo = GSON_tUINT8;  // u8
				if(valor_int<=INT8_MAX)
				{
					flag_neg_ok=1;
				}
			}
			else if(valor_int<=UINT16_MAX)
			{
				tipo = GSON_tUINT16;  // u16
				if(valor_int<=INT16_MAX)
				{
					flag_neg_ok=1;
				}
			}
			else if(valor_int<=UINT32_MAX)
			{
				tipo = GSON_tUINT32;  // u32
				if(valor_int<=INT32_MAX)
				{
					flag_neg_ok=1;
				}
			}
			else
			{
				if(valor_int<=INT64_MAX)
				{
					flag_neg_ok=1;
				}
				tipo = GSON_tUINT64;  // u64
			}
			// se 'flag_neg_ok' == 1 significa que esse inteiro positivo cabe dentro do 'signed intX' dele...
		}
	}

	*flag_neg_ok_ = flag_neg_ok;  // 0=indica que o valor nao cabe no limite positivo do sX 'signed' do unsigned em questão..., 1=o valor em questao cabe dentro do seu valor no tipo 'signed'

	// IMPAR=unsigned e PAR=signed
	return tipo;  // 0=erro, 'e_TIPOS2_GILSON'...
}



/*
static void *get_num_limit(const int tipo_ori)
{
	uint8_t num[8]={0};  // pior caso que ocupa 8 bytes!
	void* ptr = NULL;

	if(tipo_ori==GSON_tINT8)
	{
		*(uint8_t*)num = INT8_MAX;
		ptr = num;
	}
	else if(tipo_ori==GSON_tUINT8)
	{
		*(uint8_t*)num = UINT8_MAX;
		ptr = num;
	}
	else if(tipo_ori==GSON_tINT16)
	{
		*(int16_t*)num = INT16_MAX;
		ptr = num;
	}
	else if(tipo_ori==GSON_tUINT16)
	{
		*(uint16_t*)num = UINT16_MAX;
		ptr = num;
	}
	else if(tipo_ori==GSON_tINT32)
	{
		*(int32_t*)num = INT32_MAX;
		ptr = num;
	}
	else if(tipo_ori==GSON_tUINT32)
	{
		*(uint32_t*)num = UINT32_MAX;
		ptr = num;
	}
	else if(tipo_ori==GSON_tINT64)
	{
		*(int64_t*)num = INT64_MAX;
		ptr = num;
	}
	else if(tipo_ori==GSON_tUINT64)
	{
		*(uint64_t*)num = UINT64_MAX;
		ptr = num;
	}
	else if(tipo_ori==GSON_tFLOAT32)
	{
		*(float*)num = 3.402823e38;  // gambiiiii
		ptr = num;
	}
	else if(tipo_ori==GSON_tFLOAT64)
	{
		*(double*)num = 1.79769313e308;  // gambiiiii
		ptr = num;
	}
	return ptr;
}


static int valid_num_limit(const uint8_t *valor, const int tipo_ori, const int tipo_valida)
{
	int flag_ok=0;
	//uint8_t num[8]={0};  // pior caso que ocupa 8 bytes!

	// com o 'tipo_valida' eu sei qual o maior valor dele permitido/limite...

	if(tipo_ori==GSON_tINT8)
	{
		//printf("valor:%i e limit:%li\n", *(int8_t*)valor, *(int64_t *)get_num_limit(tipo_valida));
		if(*(int8_t*)valor <= INT8_MAX)
		{
			flag_ok = 1;
		}
	}
	else if(tipo_ori==GSON_tUINT8)
	{
		if(*(uint8_t*)valor <= UINT8_MAX)
		{
			flag_ok = 1;
		}
	}
	else if(tipo_ori==GSON_tINT16)
	{
		if(*(int16_t*)valor <= INT16_MAX)
		{
			flag_ok = 1;
		}
	}
	else if(tipo_ori==GSON_tUINT16)
	{
		if(*(uint16_t*)valor <= UINT16_MAX)
		{
			flag_ok = 1;
		}
	}
	else if(tipo_ori==GSON_tINT32)
	{
		if(*(int32_t*)valor <= INT32_MAX)
		{
			flag_ok = 1;
		}
	}
	else if(tipo_ori==GSON_tUINT32)
	{
		if(*(uint32_t*)valor <= UINT32_MAX)
		{
			flag_ok = 1;
		}
	}
	else if(tipo_ori==GSON_tINT64)
	{
		if(*(int64_t*)valor <= INT64_MAX)
		{
			flag_ok = 1;
		}
	}
	else if(tipo_ori==GSON_tUINT64)
	{
		if(*(uint64_t*)valor <= UINT64_MAX)
		{
			flag_ok = 1;
		}
	}
	else if(tipo_ori==GSON_tFLOAT32)
	{
		//  *(float*)num = 3.402823e38;  // gambiiiii
		//ptr = num;
		if(*(float*)valor <= 3.402823e38)
		{
			flag_ok = 1;
		}
	}
	else if(tipo_ori==GSON_tFLOAT64)
	{
		//  *(double*)num = 1.79769313e308;  // gambiiiii
		//ptr = num;
		if(*(double*)valor <= 1.79769313e308)
		{
			flag_ok = 1;
		}
	}
	return flag_ok;
}

static int check_num_limit(const char *num, const int tipo_ori, const int tipo_check)
{
	int num_ok=0;
	if(tipo_ori==GSON_tINT8)
	{
		const int8_t valor = (int8_t)atoi(num);
		//num_ok = valid_num_limit((uint8_t *)&valor, tipo_ori, tipo_check);
		//chama aqui mesmooo
	}
	else if(tipo_ori==GSON_tUINT8)
	{
		const uint8_t valor = (uint8_t)atoi(num);
		num_ok = valid_num_limit((uint8_t *)&valor, tipo_ori, tipo_check);
	}
	else if(tipo_ori==GSON_tINT16)
	{
		const int16_t valor = (int16_t)atoi(num);
		num_ok = valid_num_limit((uint8_t *)&valor, tipo_ori, tipo_check);
	}
	else if(tipo_ori==GSON_tUINT16)
	{
		const uint16_t valor = (uint16_t)atoi(num);
		num_ok = valid_num_limit((uint8_t *)&valor, tipo_ori, tipo_check);
	}
	else if(tipo_ori==GSON_tINT32)
	{
		const int32_t valor = (int32_t)atol(num);
		num_ok = valid_num_limit((uint8_t *)&valor, tipo_ori, tipo_check);
	}
	else if(tipo_ori==GSON_tUINT32)
	{
		const uint32_t valor = (uint32_t)atoll(num);
		num_ok = valid_num_limit((uint8_t *)&valor, tipo_ori, tipo_check);
	}
	else if(tipo_ori==GSON_tINT64)
	{
		char *endptr;
		const int64_t valor = (int64_t)strtoll(num, &endptr, 10);
		num_ok = valid_num_limit((uint8_t *)&valor, tipo_ori, tipo_check);
	}
	else if(tipo_ori==GSON_tUINT64)
	{
		char *endptr;
		//errno = 0;  // Limpa possíveis erros anteriores
		const uint64_t valor = (uint64_t)strtoull(num, &endptr, 10);
		num_ok = valid_num_limit((uint8_t *)&valor, tipo_ori, tipo_check);
	}
	else if(tipo_ori==GSON_tFLOAT32)
	{
		const float valor = (float)atof(num);
		num_ok = valid_num_limit((uint8_t *)&valor, tipo_ori, tipo_check);
	}
	else if(tipo_ori==GSON_tFLOAT64)
	{
		const double valor = (double)atof(num);
		num_ok = valid_num_limit((uint8_t *)&valor, tipo_ori, tipo_check);
	}
	return num_ok;
}
*/

int get_nbytes(const uint8_t tipo2)
{
	int nbytes=0;
	switch(tipo2)
	{
	case GSON_tBIT:
		// nada ainda...
		nbytes = 1;
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
	default:
		nbytes = 1;
	}

	return nbytes;
}

static int info_lista(const char *str_lista, const int len, const int barras_externo, int *size_max_pack_temp)
{
	int i, j, cont_flut=0, cont_neg=0, cont_aspas=0, cont_virgula=0, cont_bar0=0, cont_bar1=0, temp1, temp2, temp3, temp4, cont_elementos1=1, cont_elementos2;
	int tipo_valor, tipo_misturado=0, tipo_atual=0, maior_tipo_num1, maior_tipo_num2, maior_tipo_num1neg, maior_tipo_num2neg, maior_tipo_def, maior_tipo_valid, flag_neg_ok, tot_bytes_v1=0, tot_bytes_v2=0, nbytes;
	int nivel_matriz[20]={0};
	uint8_t flag_flut=0, flag_neg=0, k;
	char buff[20]={0}, sms_nivel_matriz[20]={0};

	cont_aspas = count_chr_str(str_lista, '"', len);

	cont_virgula = count_chr_str(str_lista, ',', len);

	cont_flut = count_chr_str(str_lista, '.', len);
	if(cont_flut)
	{
		// temos lista(s) com flutuantes
		flag_flut=1;
	}
	else
	{
		// somente de inteiros

	}

	cont_neg = count_chr_str(str_lista, '-', len);
	if(cont_neg)
	{
		// temos lista(s) com negativos
		flag_neg=1;
	}
	else
	{
		// somente positivos

	}

	cont_bar0 = count_chr_str(str_lista, '[', len);
	cont_bar1 = count_chr_str(str_lista, ']', len);
	// tem que bater igual....

	if(barras_externo>7)
	{
		// matriz fora da faixa analisada, dai tu qué mata o galo veio tchee
		return -1;
	}

	temp1 = barras_externo-1;
	temp3 = 1;
	temp4 = 1;
	k=0;
	while(1)
	{
		memset(buff, 0x00, sizeof(buff));
		memset(buff, ']', temp1);
		buff[temp1] = ',';
		memset(&buff[temp1+1], '[', temp1);

		temp2 = count_str_str(str_lista, buff, len);

		/*
		if(k==0 && barras_externo!=1 &&(temp2+1)!=barras_externo)
		{
			// erro grave, nao faz sentido o inicio com a busca, 'barras_externo' X primeira busca
			return -2;
		}
		else if(k>0)
		{
			temp4 = (temp2+1)/temp3;
		}
		else
		{
			temp4 = temp2+1;
		}
		*/
		if(k>0)
		{
			temp4 = (temp2+1)/temp3;
		}
		else
		{
			temp4 = temp2+1;
		}

		cont_elementos1 *= temp4;
		nivel_matriz[k] = temp4;
		k+=1;

		//k += sprintf(&sms_nivel_matriz[k], "[%u]", temp4);

		temp1-=1;

		if(temp1<0) break;

		temp3 *= temp4;
	}


	if(cont_aspas)
	{
		// temos lista de strings???
		// se nossa lista é puramente de strings entao devemos ter que 'cont_elementos1 == (cont_aspas/2)' caso contrário é misturasss
		if(cont_elementos1 != (cont_aspas/2))
		{
			//printf("ERRO cont_elementos1:%u, cont_aspas:%u\n", cont_elementos1, cont_aspas);
			//return -2;

			tipo_misturado=1;  // temos uma lista formado por 'string' e 'números'
		}
	}


	// vamos varrer todos os 'cont_elementos' e detectar qual tipo de cada lista da matriz porem temos que padronizar um tipo entao se tiver 1 float, tudo vai ser float, se tudo fora <256 tudo vai ser u8...
	tot_bytes_v1=0;
	i=0;
	j=0;
	cont_elementos2=0;
	maior_tipo_num1=0;
	maior_tipo_num2=0;
	maior_tipo_num1neg=0;
	maior_tipo_num2neg=0;
	tipo_atual=0;  // 0=nada, 1=numero, 2=string
	memset(buff, 0x00, sizeof(buff));
	while(1)
	{
		if(str_lista[i] == '[' || str_lista[i] == ']' || str_lista[i] == ',')
		{
			i+=1;
			if(j)
			{
				if(tipo_atual==1)//if(cont_aspas==0)
				{
					flag_neg_ok=0;
					tipo_valor = info_numero(buff, &flag_neg_ok);
					printf("======NUM LISTA:|%s| (tipo=%u neg_ok:%u)\n", buff, tipo_valor, flag_neg_ok);
					memset(buff, 0x00, sizeof(buff));

					//-----------------------------------------------------
					if(tipo_valor%2==0 && tipo_valor>maior_tipo_num1)
					{
						// pares (temos negativos)
						maior_tipo_num1 = tipo_valor;
						maior_tipo_num1neg = flag_neg_ok;
					}
					if(tipo_valor%2!=0 && (tipo_valor>maior_tipo_num2 || (tipo_valor>=maior_tipo_num2 && flag_neg_ok==0)))
					{
						// impares (tempos positivos)
						maior_tipo_num2 = tipo_valor;
						maior_tipo_num2neg = flag_neg_ok;
						//printf("===================::: positivo:%u(%u)\n", maior_tipo_num2, maior_tipo_num2neg);
					}
					// caso seja float... ignora isso pois temos f32 e f64 que envolve + e -
					//-----------------------------------------------------
					//printf("maiores::: negativo:%u(%u), positivo:%u(%u)\n", maior_tipo_num1, maior_tipo_num1neg, maior_tipo_num2, maior_tipo_num2neg);

					nbytes = get_nbytes(tipo_valor);
					tot_bytes_v1 += (1+nbytes);
				}
				else if(tipo_atual==2)
				{
					tipo_valor = GSON_tSTRING;
					printf("======STR LISTA:|%.*s|\n", j, &str_lista[temp1]);

					if(j>LEN_MAX_STRING)
					{
						return -3;
					}

					tot_bytes_v1 += (1+j-2);  // desconta as aspas e soma o bytes de tamanho
				}

				j=0;
				cont_elementos2+=1;
				tipo_atual=0;
			}
		}

		if((tipo_atual==0 || tipo_atual==1) && ((str_lista[i] >= '0' && str_lista[i] <= '9') || str_lista[i] == '-' || str_lista[i] == '.'))
		{
			tipo_atual=1;
			buff[j] = str_lista[i];
			j+=1;
		}
		else if((tipo_atual==0 || tipo_atual==2) && (str_lista[i] != '[' && str_lista[i] != ']' && str_lista[i] != ','))
		{
			tipo_atual=2;
			if(j==0)
			{
				temp1 = i;
			}
			j+=1;
		}



		i+=1;
		if(i>=len) break;
	}

	tot_bytes_v1 += 3;  // identificador, quantidade 2B

	// debug
	for(i=0, j=0; i<k; i++)
	{
		j += sprintf(&sms_nivel_matriz[j], "[%u]", nivel_matriz[i]);
	}
	printf("info_lista::: (barras_ext:%u, flu:%u, neg:%u, aspas:%u, virgula:%u, nivel_matriz:%s, cont_elementos:%u/%u, cont[]:%u/%u, maior_tipo_num(PeI):%u(%u)|%u(%u), tot_bytes:%u) LISTA:|%.*s|\n", barras_externo, flag_flut, flag_neg, cont_aspas, cont_virgula, sms_nivel_matriz, cont_elementos1, cont_elementos2, cont_bar0, cont_bar1, maior_tipo_num1, maior_tipo_num1neg, maior_tipo_num2, maior_tipo_num2neg, tot_bytes_v1, len, str_lista);

	//----------------------------------------------------------------------------------------------------
	//----------------------------------------------------------------------------------------------------
	if(cont_aspas==0 && tipo_misturado==0)
	{
		// etapa finallllll
		// temos o maior PAR e maior IMPAR, em maior_tipo_num1, maior_tipo_num2 respectivamente... agora basta saber se todos os numeros cabem nesse 'tipo' escolhido!!!
		// check_num_limit()
		/*
		if(maior_tipo_num1 > maior_tipo_num2)
		{
			maior_tipo_def = maior_tipo_num1;
		}
		else
		{
			maior_tipo_def = maior_tipo_num2;
		}
		*/
		if(flag_flut)
		{
			if(maior_tipo_num1 > maior_tipo_num2)
			{
				maior_tipo_def = maior_tipo_num1;
			}
			else
			{
				maior_tipo_def = maior_tipo_num2;
			}
		}
		else if(maior_tipo_num1==0)  // ou 'flag_neg==1'
		{
			// só temos numero positivos
			maior_tipo_def = maior_tipo_num2;
		}
		else
		{
			// temos inteiros com positivos e negativos
			if(maior_tipo_num1 > maior_tipo_num2)
			{
				maior_tipo_def = maior_tipo_num1;  // ta tudo certooo, pois nenhum positivo passou desse 'maior' negativo, isto é, o 'maior positivo' cabe dentro do 'maior negativo'
			}
			else
			{
				//maior_tipo_def = maior_tipo_num2;  // isso pode complicar pois o 'maior negativo' pode ser que nao caiba dentro do 'maior positivo'
				if(maior_tipo_num2-maior_tipo_num1 == 1 && maior_tipo_num2neg==1)
				{
					maior_tipo_def = maior_tipo_num1;  // tudo certooo pois o 'maior positivo' cabe dentro do 'maior negativo'
				}
				else
				{
					maior_tipo_def = maior_tipo_num2+1;
					if(maior_tipo_def>=GSON_tFLOAT32)
					{
						// malucooo nao tem comooo....
						return -4;
					}
				}
			}
		}

		tot_bytes_v1=0;
		tot_bytes_v2=0;
		i=0;
		j=0;
		cont_elementos1=0;
		cont_elementos2=0;
		memset(buff, 0x00, sizeof(buff));
		while(1)
		{
			if(str_lista[i] == '[' || str_lista[i] == ']' || str_lista[i] == ',')
			{
				i+=1;
				if(j)
				{
					if(cont_aspas==0)
					{
						flag_neg_ok=0;
						tipo_valor = info_numero(buff, &flag_neg_ok);
						//printf("======VALOR:|%s| (tipo=%u neg_ok:%u)\n", buff, tipo_num, flag_neg_ok);
						/*
						// vamos ter certeza que o nosso 'maior_tipo_def' escolhido vai alocar corretamente todos os nossos dados!!!
						maior_tipo_valid = check_num_limit(buff, tipo_num, maior_tipo_def);
						if(maior_tipo_valid)
						{
							cont_elementos1+=1;
						}
						*/


						nbytes = get_nbytes(tipo_valor);
						tot_bytes_v1 += (1+nbytes);

						memset(buff, 0x00, sizeof(buff));
					}
					else
					{
						//printf("======VALOR:|%.*s|\n", j, &str_lista[temp1]);
					}
					j=0;
					cont_elementos2+=1;
				}
			}

			if(cont_aspas==0)
			{
				if((str_lista[i] >= '0' && str_lista[i] <= '9') || str_lista[i] == '-' || str_lista[i] == '.')
				{
					buff[j] = str_lista[i];
					j+=1;
				}
			}
			else
			{
				if(str_lista[i] != '[' && str_lista[i] != ']' && str_lista[i] != ',')
				{
					if(j==0)
					{
						temp1 = i;
					}
					j+=1;
				}
			}

			i+=1;
			if(i>=len) break;
		}
		tot_bytes_v1 += 3;  // identificador, quantidade 2B

		nbytes = get_nbytes(maior_tipo_def);
		tot_bytes_v2 = (nbytes*cont_elementos2)+3;  // +1 = identificador, quantidade 2B

		printf("\tmaior_tipo_def:%u, cont_elementos2:%u == cont_elementos1:%u, tot_bytes:%u|%u\n", maior_tipo_def, cont_elementos2, cont_elementos1, tot_bytes_v1, tot_bytes_v2);
	}
	//----------------------------------------------------------------------------------------------------
	//----------------------------------------------------------------------------------------------------

	if(cont_aspas==0 && tipo_misturado==0)
	{
		if(tot_bytes_v1 > tot_bytes_v2)
		{
			*size_max_pack_temp = tot_bytes_v2;
		}
		else
		{
			*size_max_pack_temp = tot_bytes_v1;
		}
	}
	else
	{
		*size_max_pack_temp = tot_bytes_v1;
	}


	return 0;
}






int32_t gilson_encode_from_json(const char *json_in, uint8_t *pack, const uint32_t size_max_pack)
{
	// testes...
	// basicamente é fazer o 'parse' do JSON para detectar chave e valor e quando temos listas ou objeto dentro de objeto e quando é string, inteiro ou flutuante e otimizar para o melhor formato em bytes
	uint64_t valor_int;
	double valor_flutuante;
	int conts[6]={0};
	int erro=0, i=0, j=0, k=0, size_json=0, cont_chaves=0, init_obj=0, init_nome_chave=0, init_nome_chave_ok=0, init_nome_valor=0, init_nome_valor_ok=0, pos=0, pos2=0, cont=0, cont_barra=0, check=0, flag_neg_ok;
	int size_max_pack2=0, size_max_pack_temp=0;
	char nome_chave[40], nome_valor[40], numero[20];

	size_json = strlen(json_in);

	if(json_in[0]!='{' || json_in[size_json-1]!='}')
	{
		erro = -1;
		goto deu_erro;
	}


	// [0] = GSON_MODO_JSON
	// [1:5] = 4B crc32
	// [5:9] = 4B len_json
	// [7:8] = 4B len_gilson
	size_max_pack2 = 13;

	// "{\"sensor\":\"supremo\",\"inteiro\":1234,\"flutuante\":665.2345,\"flag\":false,\"perdido\":null}";
	// OBS: 'json_in' deve estar compactado no sentido de nao ter espaço, quebra de linha, caracteres <=32 onde 32=' '

	// pendências....
	// {[1, "texto", true, null, {"chave": "valor"}]}
	// {[{"nome": "Alice", "idade": 30}, {"nome": "Bob", "idade": 25}]}
	// {[1, 2, [3, 4, 5], 6]}

	// validação inicial
	for(i=0; i<size_json; i++)
	{
		// atualmente não vamos tratar JSON com outros JSON dentro ou o que se chama de objetos dentro...
		if((uint8_t)json_in[i]=='{')
		{
			conts[0]+=1;
			/*
			if(cont>1)  // só aceita 1, que faz parte de
			{
				erro = -2000;
				goto deu_erro;
			}
			*/
		}

		// atualmente não vamos tratar JSON com outros JSON dentro ou o que se chama de objetos dentro...
		if((uint8_t)json_in[i]=='}')
		{
			conts[1]+=1;
			/*
			if(cont1>1)  // só aceita 1, que faz parte de
			{
				erro = -2001;
				goto deu_erro;
			}
			*/
		}

		// da tabela ascii deve ser >32, nao pode ter caracteres de controle, nem os \r \n \t..., veremos no futuro...
		if((uint8_t)json_in[i]<32)
		{
			if(conts[2]==0)
			{
				pos = i;
			}
			conts[2]+=1;
			//printf("json_in[%u]=|%c| (%u) amostra:|%.20s|\n", i, json_in[i], json_in[i], &json_in[i]);
			//erro = -1000;
			//goto deu_erro;
		}

		if((uint8_t)json_in[i]=='[')
		{
			conts[3]+=1;
		}
		if((uint8_t)json_in[i]==']')
		{
			conts[4]+=1;
		}
	}

	// debug
	printf("estatisticas::: conts:(%u, %u, %u, %u, %u) json_in[%u]=|%c| (%u) amostra:|%.20s|\n", conts[0], conts[1], conts[2], conts[3], conts[4], pos, json_in[pos], json_in[pos], &json_in[pos]);
	if(conts[0]>1 || conts[1]>1 || conts[2]>0 || (conts[3]!=conts[4]))
	{
		erro = -1000;
		goto deu_erro;
	}




	i=0;
	cont=0;
	while(1)//for(i=0; i<size_json; i++)
	{
		if(json_in[i]=='{')
		{
			init_obj += 1;
		}

		if(json_in[i]=='"')
		{
			if(init_nome_chave==0 && init_nome_chave_ok==0)
			{
				init_nome_chave = 1;
				j=0;
				memset(nome_chave, 0x00, sizeof(nome_chave));
				i+=1;  // avança para próximo ja...
			}
			else if(init_nome_chave==1 && init_nome_chave_ok==0)
			{
				init_nome_chave = 0;
				init_nome_chave_ok = 1;
			}
		}

		if(init_nome_chave==1 && init_nome_chave_ok==0)
		{
			nome_chave[j] = json_in[i];
			j+=1;
		}
		if(init_nome_chave_ok==1)
		{
			init_nome_chave_ok = 2;  // fica pronto para detectar o valor da chave...
			// feito o nome da chave
			printf("===CHAVE:|%s| %u bytes\n", nome_chave, j);
			size_max_pack2 += (1 + j);
		}

		if(json_in[i]==':')
		{
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			if(json_in[i+1] == '"')
			{
				// ================================= temos VALOR: string
				// descobre o fim da string com '"'
				pos = pos_str_chr(&json_in[i+2], '"');
				if(pos==-1)
				{
					// nao achou o final da string nome da chave
					erro = -2;
					goto deu_erro;
				}
				memset(nome_valor, 0x00, sizeof(nome_valor));
				strncpy(nome_valor, &json_in[i+2], pos);
				printf("===VALOR:|%s| %u bytes\n", nome_valor, pos);
				size_max_pack2 += (2 + j);
				i+=(pos+3);  // ja desconta as aspas duplas da string
			}
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			else if(json_in[i+1] == '-' || (json_in[i+1]>=0x30 && json_in[i+1]<=0x39))
			{
				// =================================  temos VALOR: número (seja inteiro ou flutuante)
				pos = pos_str_chr(&json_in[i+1], ',');
				if(pos==-1)
				{
					// pode ser que chegou no final onde nao vai ter ','
					pos = pos_str_chr(&json_in[i+1], '}');
					if(pos==-1)
					{
						erro = -3;
						goto deu_erro;
					}
				}
				memset(numero, 0x00, sizeof(numero));
				strncpy(numero, &json_in[i+1], pos);
				flag_neg_ok=0;
				size_max_pack_temp=0;
				check = info_numero(numero, &flag_neg_ok);
				printf("======VALOR:|%s| (tipo=%u, flag_neg_ok:%u)\n", numero, check, flag_neg_ok);
				size_max_pack_temp = get_nbytes(check);
				size_max_pack2 += (1 + size_max_pack_temp);
				// tem '.' nisso? se sim é um flutuante
				// quantos algarismos tem? até 255, 65535, 2**32-1, u64
				// é negativo? (-128 a 127), (-32.768 a 32.767), (-2.147.483.648 a 2.147.483.647) s64
				// float ou double? se total de casas decimais for >5~6 entao passa para double, ou entao se a parte inteira for >10e7 entao é double

				i+=(pos+1);
			}
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			else if(json_in[i+1] == '[')
			{
				// =================================   temos VALOR: lista (pode ser multiplos tipos... string, inteiro, flutuante, tudo misturado)
				// qual tipo de dados dessa lista???
				// caso seja multiplas matrizes de n dimensões... busca por "]," ou "]}"
				// OBS: limite até [x][y] matriz dupla!!!!!!!! o GILSON
				pos = pos_str_chr(&json_in[i+1], ']');  // vamos achar o primeiro fechada de lista... porem sabemos que tem varios tipos de lista e listas dentro de listas...
				if(pos==-1)
				{
					// não achou o final da lista, temos problema!!!
					erro = -4;
					goto deu_erro;
				}
				cont_barra=0;
				for(k=0; k<pos; k++)
				{
					if(json_in[(i+1)+k] == '[')
					{
						cont_barra+=1;
					}
					else
					{
						// achou o primeiro diferente de '[' sai fora
						break;
					}
					/*
					memset(nome_valor, 0x00, sizeof(nome_valor));
					memset(nome_valor, ']', cont_barra);
					pos = pos_str_str(&json_in[i+1], nome_valor);  // vamos achar final da lista
					if(pos==-1)
					{
						erro = -400;
						goto deu_erro;
					}
					pos2=cont_barra;
					*/
				}
				memset(nome_valor, 0x00, sizeof(nome_valor));
				memset(nome_valor, ']', cont_barra);
				pos = pos_str_str(&json_in[i+1], nome_valor);  // vamos achar final da lista
				if(pos==-1)
				{
					// não achou o final da lista multipla respectiva do seu início, temos problema!!!
					erro = -40;
					goto deu_erro;
				}
				pos2=cont_barra;
				// OBS: (pos+pos2) = final da lista na string geral do json
				//memset(lista_loco, 0x00, sizeof(lista_loco));
				//strncpy(lista_loco, &json_in[i+1], (pos+pos2));
				if(pos2==1)
				{
					// lista simples
					cont = count_chr_str(&json_in[i+1], ',', (pos+pos2)) + 1;
				}
				else
				{
					// lista dupla ou maior, OBS: nesse caso nao é válido pois temos muitos tipos de matrizes e possibilidades, tem que analisar mais profundo
					cont = count_str_str(&json_in[i+1], "],", (pos+pos2)) + 1;
				}
				check = info_lista(&json_in[i+1], (pos+pos2), cont_barra, &size_max_pack_temp);
				printf("======VALOR LISTA (tipo:%u, %u itens? check:%i size_max_pack_temp:%u):|%.*s|\n", pos2, cont, check, size_max_pack_temp, (pos+pos2), &json_in[i+1]);
				if(check!=0)
				{
					erro = -41;
					goto deu_erro;
				}
				size_max_pack2 += (1 + size_max_pack_temp);

				i+=(pos+pos2);  // ja desconta '[' e ']'
			}
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			else if(json_in[i+1] == 't')
			{
				// ================================= provável um 'true' 0
				pos = pos_str_chr(&json_in[i+1], ',');
				if(pos==-1)
				{
					// pode ser que chegou no final onde nao vai ter ','
					pos = pos_str_chr(&json_in[i+1], '}');
					if(pos==-1)
					{
						erro = -5;
						goto deu_erro;
					}
				}
				memset(nome_valor, 0x00, sizeof(nome_valor));
				strncpy(nome_valor, &json_in[i+1], pos);
				check = compara_str(nome_valor, "true");
				if(check==0)
				{
					erro = -50;
					goto deu_erro;
				}
				printf("======VALOR:|%s| check:%u\n", nome_valor, check);
				size_max_pack2 += (1 + 1);
				i+=(pos+1);
			}
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			else if(json_in[i+1] == 'f')
			{
				// ================================= provável um 'false' 1
				pos = pos_str_chr(&json_in[i+1], ',');
				if(pos==-1)
				{
					// pode ser que chegou no final onde nao vai ter ','
					pos = pos_str_chr(&json_in[i+1], '}');
					if(pos==-1)
					{
						erro = -6;
						goto deu_erro;
					}
				}
				memset(nome_valor, 0x00, sizeof(nome_valor));
				strncpy(nome_valor, &json_in[i+1], pos);
				check = compara_str(nome_valor, "false");
				if(check==0)
				{
					erro = -60;
					goto deu_erro;
				}
				printf("======VALOR:|%s| check:%u\n", nome_valor, check);
				size_max_pack2 += (1 + 1);
				i+=(pos+1);
			}
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			else if(json_in[i+1] == 'n')
			{
				// ================================= provável um 'null' 2
				pos = pos_str_chr(&json_in[i+1], ',');
				if(pos==-1)
				{
					// pode ser que chegou no final onde nao vai ter ','
					pos = pos_str_chr(&json_in[i+1], '}');
					if(pos==-1)
					{
						erro = -7;
						goto deu_erro;
					}
				}
				memset(nome_valor, 0x00, sizeof(nome_valor));
				strncpy(nome_valor, &json_in[i+1], pos);
				check = compara_str(nome_valor, "null");
				if(check==0)
				{
					erro = -70;
					goto deu_erro;
				}
				printf("======VALOR:|%s| check:%u\n", nome_valor, check);
				size_max_pack2 += (1 + 1);
				i+=(pos+1);
			}
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			else if(json_in[i+1] == '{')
			{
				// temos um objeto ou entao json dentro de json...
				// ainda não é tratado...
				erro = -8;
				goto deu_erro;
			}
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			else
			{
				// erro desconhecido...
				erro = -9;
				goto deu_erro;
			}
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		}

		if(json_in[i]==',')
		{
			// só vaiiiii, passa para próxima chave
			if(init_nome_chave_ok==2)
			{
				init_nome_chave_ok = 0;  // fecho o pastel do valor da chave!
			}

			cont_chaves += 1;
			printf("cont_chaves:%u, proxima chave...\n", cont_chaves);
		}

		if(json_in[i]=='}')
		{
			init_obj -= 1;

			// só vaiiiii, passa para próxima chave
			if(init_nome_chave_ok==2)
			{
				init_nome_chave_ok = 0;  // fecho o pastel do valor da chave!

				cont_chaves += 1;
				printf("fimmmm, cont_chaves:%u\n", cont_chaves);
			}
		}

		i+=1;
		if(i>=size_json)
		{
			break;
		}
	}

	deu_erro:

	printf("FIMMMMMMMMMMMMM erro:%i, size: in:%u|gilson:%u|json:%u\n", erro, size_max_pack, size_max_pack2, size_json);

	return erro;
}

int32_t gilson_decode_to_json(uint8_t *pack, char *json_out)
{
	// modo == GSON_MODO_KV, se nao for esse... tem que passar o mapa e os nomes das chaves... ou caso seja GSON_MODO_FULL somente os nomes das chaves

	//GSON_MODO_JSON

	return 0;
}

