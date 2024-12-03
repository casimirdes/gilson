/*
 * gilson.c
 *
 *  Created on: 30 de out. de 2024
 *      Author: mjm
 */


#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "crypto/crypto.h"


#include "gilson.h"

#define USO_DEBUG_LIB		1  // 0=microcontrolador, 1=PC

#define LEN_PACKS_GILSON	2  // 2 pacotes manipulaveis "ao mesmo tempo", se abriu o segundo tem que fechar para voltar para o primeiro!!!

#define GSON_tLIST		64
#define GSON_tMTX2D		65

#define OFFSET_MODO_ZIP		1
#define OFFSET_MODO_FULL	8


// estrutura de controle da lib gilson
typedef struct {
	uint32_t end_ram;
	uint32_t crc;
	uint32_t crc_out;  // quando decodifica
	uint16_t pos_bytes, pos_bytes2, pos_bytes_bk;
	uint8_t modo;  // 0=padrao, 1=compacto
	uint8_t ativo;
	uint8_t cont_itens, cont_itens2, cont_itens_old;
	uint8_t chave_atual;  // para fins de comparar e validar chaves durante encode
	uint8_t *bufw;  // buffer de escrita/leitura
	const uint8_t *bufr;  // buffer somente leitura
}struct_gilson;


static struct_gilson s_gil[LEN_PACKS_GILSON]={0};
static uint8_t ig=0;



//========================================================================================================================
//========================================================================================================================
//========================================================================================================================
//========================================================================================================================
// PARTE DE CODIFICAÇÃO

int32_t gilson_encode_init(const uint8_t modo_, uint8_t *pack)
{
	uint32_t end_ram=0;
	int32_t erro=0;

	// queremos codificar um pacote...

	if(s_gil[ig].ativo==1)
	{
		erro = -1;
		end_ram = (uint32_t)pack;  // endereço na ram????

		// atual 'ig' está ativo ja... nao tem o que fazer... vamos consultar o segundo
		if(ig==0)
		{
			// vamos ver o segundo...
			if(s_gil[ig+1].ativo==1)
			{
				erro = -2;
				// nao tem o que fazer.... aborta a missao!!!!
			}
			else if(end_ram==s_gil[ig].end_ram)
			{
				// quer utililzar o mesmo vetor geral????? nao tem fazer isso....
				erro = -3;
				// nao tem o que fazer.... aborta a missao!!!!
			}
			else
			{
				erro=0;
				ig=1;  // vai para o segundo e esse passar ser o atual global da lib
			}
		}

		if(erro!=0) goto deu_erro;
	}

	memset(&s_gil[ig], 0x00, sizeof(s_gil[ig]));

	s_gil[ig].modo = modo_;
	s_gil[ig].bufw = pack;
	s_gil[ig].end_ram = (uint32_t)pack;  // endereço na ram????

	memset(s_gil[ig].bufw, 0x00, sizeof(*s_gil[ig].bufw));

	s_gil[ig].cont_itens=0;

	if(s_gil[ig].modo == GSON_MODO_FULL)
	{
		s_gil[ig].pos_bytes = OFFSET_MODO_FULL;  // offset
	}
	else
	{
		s_gil[ig].pos_bytes = OFFSET_MODO_ZIP;  // offset
	}

	deu_erro:

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

	if(s_gil[ig].modo == GSON_MODO_FULL)
	{
		memcpy(&s_gil[ig].bufw[5], &s_gil[ig].pos_bytes, 2);
		s_gil[ig].bufw[7] = s_gil[ig].cont_itens;

		s_gil[ig].crc = fs_crc(0xffffffff, &s_gil[ig].bufw[5], s_gil[ig].pos_bytes-5);

		memcpy(&s_gil[ig].bufw[1], &s_gil[ig].crc, 4);
	}
	else  // GSON_MODO_ZIP
	{
		s_gil[ig].crc = fs_crc(0xffffffff, s_gil[ig].bufw, s_gil[ig].pos_bytes);  // chamar depois que 'pos_bytes' está completo!!!!
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

/*
uint32_t gilson_encode_end_crc(void)
{
	// deve ser chamado depois de 'gilson_encode_end()' pois tem o valor atualizado de 'crc'
	// OBS: grande problema aqui é que vai pegar o crc do 'ig' da vez.. nao tem como escolher o do outro!!!!!!********

#if (USO_DEBUG_LIB==1)
	printf("DEBUG gilson_encode_end_crc::: ig:%u, modo:%u, crc:%u, pos_bytes:%u\n", ig, s_gil[ig].modo, s_gil[ig].crc, s_gil[ig].pos_bytes);
#endif

	return s_gil[ig].crc;
}
*/


// colocar sempre o "(uint8_t *)" na frente da variavel de entrada 'multi_data' e passar como "&"
int32_t gilson_encode_data(const uint8_t chave, const uint8_t tipo1, const uint8_t tipo2, const uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	int32_t erro=0, len;
	uint16_t i, vezes = 1, nbytes = 1, temp16;
	uint8_t len_string_max=0;
	char buff[256];  // supondo tamanho maximo de strings

	if(s_gil[ig].ativo==0)
	{
		erro = -4;
		goto deu_erro;
	}

	if(tipo1>=GSON_MAX || tipo2>=GSON_tMAX)
	{
		erro = -5;
		goto deu_erro;
	}

	if(chave > s_gil[ig].cont_itens)
	{
		// quer add uma chave maior do que a contagem crescente... nao tem como
		erro = -6;
		goto deu_erro;
	}

	if(tipo1 == GSON_LIST)
	{
		if(cont_list_a == 0)
		{
			erro = -6;
			goto deu_erro;
		}
		vezes = cont_list_a;

		if(s_gil[ig].modo == GSON_MODO_FULL)
		{
			s_gil[ig].bufw[s_gil[ig].pos_bytes] = GSON_tLIST;
			s_gil[ig].pos_bytes += 1;
			s_gil[ig].bufw[s_gil[ig].pos_bytes] = tipo2;
			s_gil[ig].pos_bytes += 1;
			memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], &vezes, 2);
			s_gil[ig].pos_bytes += 2;
			//printf("DEBUG gilson_encode_data: [%u,%u,%u,%u]\n", bufw[pos_bytes-4], bufw[pos_bytes-3], bufw[pos_bytes-2], bufw[pos_bytes-1]);
		}

		if(tipo2==GSON_tSTRING)
		{
			// aqui 'cont_list_b' é tratado como uint8

			if(cont_list_b==0)
			{
				// precisamos do offset cru da lista de strings...
				erro = -7;
				goto deu_erro;
			}
			else
			{
				len_string_max = cont_list_b;
				if(s_gil[ig].modo == GSON_MODO_FULL)
				{
					s_gil[ig].bufw[s_gil[ig].pos_bytes] = len_string_max;
					s_gil[ig].pos_bytes += 1;
				}
			}
		}
	}
	else if(tipo1 == GSON_MTX2D)
	{
		// aqui 'cont_list_b' é tratado como uint16

		if(cont_list_a == 0 || cont_list_b == 0 || cont_list_step == 0)
		{
			erro = -8;
			goto deu_erro;
		}
		//vezes = cont_list_a * cont_list_b;  nao utiliza...

		if(s_gil[ig].modo == GSON_MODO_FULL)
		{
			s_gil[ig].bufw[s_gil[ig].pos_bytes] = GSON_tMTX2D;
			s_gil[ig].pos_bytes += 1;
			s_gil[ig].bufw[s_gil[ig].pos_bytes] = tipo2;
			s_gil[ig].pos_bytes += 1;
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
		if(s_gil[ig].modo == GSON_MODO_FULL)
		{
			s_gil[ig].bufw[s_gil[ig].pos_bytes] = tipo2;
			s_gil[ig].pos_bytes += 1;
		}
		vezes = 1;

		if(tipo2==GSON_tSTRING)
		{
			if(cont_list_b==0)
			{
				erro = -9;
				goto deu_erro;
			}
			else
			{
				len_string_max = cont_list_b;
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
		//memcpy(&bufw[pos_bytes], multi_data, 1*vezes);
		//pos_bytes += 1*vezes;
		break;
	case GSON_tINT16:
	case GSON_tUINT16:
		nbytes = 2;
		//memcpy(&bufw[pos_bytes], multi_data, 2*vezes);
		//pos_bytes += 2*vezes;
		break;
	case GSON_tINT32:
	case GSON_tUINT32:
		nbytes = 4;
		//memcpy(&bufw[pos_bytes], multi_data, 4*vezes);
		//pos_bytes += 4*vezes;
		break;
	case GSON_tINT64:
	case GSON_tUINT64:
		nbytes = 8;
		//memcpy(&bufw[pos_bytes], multi_data, 8*vezes);
		//pos_bytes += 8*vezes;
		break;
	case GSON_tFLOAT32:
		nbytes = 4;
		//memcpy(&bufw[pos_bytes], multi_data, 4*vezes);
		//printf("DEBUG gilson_encode_data: [%u,%u,%u,%u]\n", bufw[pos_bytes], bufw[pos_bytes+1], bufw[pos_bytes+2], bufw[pos_bytes+3]);
		//pos_bytes += 4*vezes;
		break;
	case GSON_tFLOAT64:
		nbytes = 8;
		//memcpy(&bufw[pos_bytes], multi_data, 8*vezes);
		//pos_bytes += 8*vezes;
		break;
	case GSON_tSTRING:
		// só vai...
		break;
	default:
		erro = -10;
		goto deu_erro;
	}


	if(tipo2 == GSON_tSTRING)
	{
		for(i=0; i<vezes; i++)
		{
			/*
			if(vezes==1)
			{
				len = strlen((char *)multi_data);
			}
			else
			{
				memcpy(buff, multi_data+(i*len_string_max), len_string_max);
				len = strlen(buff);
			}
			*/
			// OBS: é obrigado a ter o 'len_string_max' que é o valor de 'cont_list_b'!!!!!!!!!!!!


			memcpy(buff, valor+(i*len_string_max), len_string_max);
			len = strlen(buff);  // vai esbarrar no primero '\0' que encontrar...
			// problema é com a codificação utf-8 que se formos truncar tem que cuidar onde truncar...

			//printf("DEBUGGGGGGGGGGGGGGGGGGGGGG len:%u, len_string_max:%u, |%s| |%s|\n", len, len_string_max, multi_data, buff);

			if(len>=len_string_max)
			{
				// nao temos '\0'???? caso len==len_string_max
				len = len_string_max;
				buff[len-1] = 0;  // para garantir o '\0'
				if(buff[len-2]>0 && (buff[len-2]<32 || buff[len-2]>126))  // tabela ascii, se caso ficou bugado em um utf-8
				{
					buff[len-2] = 0;
				}
				len = strlen(buff);  // atualiza os ajustes
			}

			if(len<=255)
			{
				s_gil[ig].bufw[s_gil[ig].pos_bytes] = (uint8_t)len;
				s_gil[ig].pos_bytes += 1;
				/*
				if(vezes==1)
				{
					memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], multi_data, len);
				}
				else
				{
					memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], buff, len);
				}
				*/
				memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], buff, len);

				s_gil[ig].pos_bytes += len;
			}
			else
			{
				erro = -11;  // nao é pra cair aqui pois o max de 'len_string_max' é 255 u8
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
				memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], valor+temp16, nbytes*cont_list_b);
				s_gil[ig].pos_bytes += nbytes*cont_list_b;
			}
		}
		else
		{
			memcpy(&s_gil[ig].bufw[s_gil[ig].pos_bytes], valor, nbytes*vezes);
			s_gil[ig].pos_bytes += nbytes*vezes;
		}
	}

	deu_erro:

	if(erro==0)
	{
		s_gil[ig].cont_itens += 1;
	}
	else
	{
#if (USO_DEBUG_LIB==1)
		printf("DEBUG gilson_encode_data::: ig:%u, ERRO:%i, modo:%u, tipo1:%u, tipo2:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u\n", ig, erro, s_gil[ig].modo, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step);
#endif
	}

	return erro;
}


//========================================================================================================================
//========================================================================================================================
//========================================================================================================================
//========================================================================================================================
// PARTE DE DECODIFICAÇÃO


int32_t gilson_decode_init(const uint8_t *pack)
{
	uint32_t crc1=0, crc2=0, end_ram=0;
	int32_t erro=0;

	// queremos decodificar um pacote...

	if(s_gil[ig].ativo==1)
	{
		erro = -12;
		end_ram = (uint32_t)pack;  // endereço na ram????

		// atual 'ig' está ativo ja... nao tem o que fazer... vamos consultar o segundo
		if(ig==0)
		{
			// vamos ver o segundo...
			if(s_gil[ig+1].ativo==1)
			{
				erro = -13;
				// nao tem o que fazer.... aborta a missao!!!!
			}
			else if(end_ram==s_gil[ig].end_ram)
			{
				// quer utililzar o mesmo vetor geral????? nao tem fazer isso....
				erro = -14;
				// nao tem o que fazer.... aborta a missao!!!!
			}
			else
			{
				erro=0;
				ig=1;  // vai para o segundo e esse passar ser o atual global da lib
			}
		}

		if(erro!=0) goto deu_erro;
	}

	memset(&s_gil[ig], 0x00, sizeof(s_gil[ig]));

	s_gil[ig].end_ram = (uint32_t)pack;  // endereço na ram????
	s_gil[ig].bufr = pack;
	s_gil[ig].modo = s_gil[ig].bufr[0];
	s_gil[ig].pos_bytes = 0;
	s_gil[ig].cont_itens = 0;

	if(s_gil[ig].modo >= GSON_MODO_MAX)
	{
		erro = -15;
		goto deu_erro;
	}
	else if(s_gil[ig].modo == GSON_MODO_FULL)
	{
		memcpy(&crc1, &s_gil[ig].bufr[1], 4);
		memcpy(&s_gil[ig].pos_bytes2, &s_gil[ig].bufr[5], 2);
		s_gil[ig].cont_itens2 = s_gil[ig].bufr[7];

		crc2 = fs_crc(0xffffffff, &s_gil[ig].bufr[5], s_gil[ig].pos_bytes2-5);

		if(crc1 != crc2)
		{
			erro = -16;
			goto deu_erro;
			//return -3;
		}

		s_gil[ig].crc_out = crc1;  // atualiza o global

		s_gil[ig].pos_bytes = OFFSET_MODO_FULL;  // offset geral
	}
	else
	{
		s_gil[ig].pos_bytes = OFFSET_MODO_ZIP;  // offset geral
	}

	deu_erro:

	if(erro==0)
	{
		s_gil[ig].ativo = 1;  // ativa decodificação
	}
	else
	{
		s_gil[ig].ativo = 0;
	}

#if (USO_DEBUG_LIB==1)
	printf("DEBUG gilson_decode_init::: ig:%u, erro:%i  modo:%u  pos_bytes:%u  cont_itens:%u  cru:[%u,%u,%u,%u,%u,%u,%u,%u]  crc:%u==%u  pos_bytes2:%u  cont_itens2:%u\n", ig, erro, s_gil[ig].modo, s_gil[ig].pos_bytes, s_gil[ig].cont_itens, s_gil[ig].bufr[0], s_gil[ig].bufr[1], s_gil[ig].bufr[2], s_gil[ig].bufr[3], s_gil[ig].bufr[4], s_gil[ig].bufr[5], s_gil[ig].bufr[6], s_gil[ig].bufr[7], crc1, crc2, s_gil[ig].pos_bytes2, s_gil[ig].cont_itens2);
#endif

	return erro;
}


int32_t gilson_decode_end(uint32_t *crc)
{
	int32_t erro=0;

	if(s_gil[ig].modo == GSON_MODO_FULL)
	{
		/*
		if(s_gil[ig].pos_bytes != s_gil[ig].pos_bytes2 && s_gil[ig].cont_itens != s_gil[ig].cont_itens2)
		{
			erro = -17;
		}
		else if(s_gil[ig].pos_bytes != s_gil[ig].pos_bytes2)
		{
			erro = -18;
		}
		else if(s_gil[ig].cont_itens != s_gil[ig].cont_itens2)
		{
			erro = -19;
		}
		*/
		if(s_gil[ig].cont_itens == s_gil[ig].cont_itens2 && s_gil[ig].pos_bytes != s_gil[ig].pos_bytes2)
		{
			// chegou até o fim 'cont_itens2' mas o 'pos_bytes' nao bateu com o 'pos_bytes2', dai deu ruim
			erro = -17;
		}
		else
		{
			// indica que nao leu todas chaves ou nao foi até a última ou nao veu em ordem até o fim
			s_gil[ig].pos_bytes = s_gil[ig].pos_bytes2;
			s_gil[ig].cont_itens = s_gil[ig].cont_itens2;
		}
	}
	else
	{
		s_gil[ig].crc_out = fs_crc(0xffffffff, s_gil[ig].bufr, s_gil[ig].pos_bytes);
	}

	s_gil[ig].ativo = 0;

	*crc = s_gil[ig].crc_out;

#if (USO_DEBUG_LIB==1)
	printf("DEBUG gilson_decode_end::: ig:%u, erro:%i, modo:%u, pos_bytes:%u==%u, cont_itens:%u==%u, crc:%u \n", ig, erro, s_gil[ig].modo, s_gil[ig].pos_bytes, s_gil[ig].pos_bytes2, s_gil[ig].cont_itens, s_gil[ig].cont_itens2, s_gil[ig].crc_out);
#endif

	if(erro==0)
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
		return erro;
	}
}

/*
uint32_t gilson_decode_end_crc(void)
{
	// deve ser chamado depois de 'gilson_decode_end()' pois tem o valor atualizado de 'crc'
	// OBS: grande problema aqui é que vai pegar o crc do 'ig' da vez.. nao tem como escolher o do outro!!!!!!********

#if (USO_DEBUG_LIB==1)
	printf("DEBUG gilson_encode_end_crc::: ig:%u, crc:%u pos_bytes:%u\n", ig, s_gil[ig].crc_out, s_gil[ig].pos_bytes);
#endif

	return s_gil[ig].crc_out;
}
*/

// colocar sempre o "(uint8_t *)" na frente da variavel de entrada 'multi_data' e passar como "&"
int32_t gilson_decode_data(const uint8_t chave, const uint8_t tipo1, const uint8_t tipo2, uint8_t *valor, const uint16_t cont_list_a, const uint16_t cont_list_b, const uint16_t cont_list_step)
{
	int32_t erro=0, len;
	uint16_t i, vezes = 1, nbytes=1, temp16;

	if(s_gil[ig].ativo==0)
	{
		erro = -20;
		goto deu_erro;
	}

	if(tipo1>=GSON_MAX || tipo2>=GSON_tMAX)
	{
		erro = -21;
		goto deu_erro;
	}

	if(chave > s_gil[ig].cont_itens)
	{
		// quer add uma chave maior do que a contagem crescente...
		// vamos varrer todas as chaves até achar a 'chave' desejada mas só funciona no modo 'GSON_MODO_FULL'!!!
		erro = -6;
		goto deu_erro;
	}

	if(tipo1 == GSON_LIST)
	{
		if(cont_list_a == 0)
		{
			erro = -22;
			goto deu_erro;
		}
		vezes = cont_list_a;

		if(s_gil[ig].modo == GSON_MODO_FULL)
		{
			//bufr[pos_bytes] = GSON_tLIST;
			s_gil[ig].pos_bytes += 1;
			//bufr[pos_bytes] = tipo2;
			s_gil[ig].pos_bytes += 1;
			//memcpy(&bufr[pos_bytes], &vezes, 2);
			s_gil[ig].pos_bytes += 2;
		}

		if(tipo2==GSON_tSTRING)
		{
			if(cont_list_b==0)
			{
				// precisamos do offset cru da lista de strings...
				erro = -23;
				goto deu_erro;
			}
			else
			{
				if(s_gil[ig].modo == GSON_MODO_FULL)
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
			erro = -24;
			goto deu_erro;
		}
		//vezes = cont_list_a * cont_list_b;

		if(s_gil[ig].modo == GSON_MODO_FULL)
		{
			//bufr[pos_bytes] = GSON_tMTX2D;
			s_gil[ig].pos_bytes += 1;
			//bufr[pos_bytes] = tipo2;
			s_gil[ig].pos_bytes += 1;
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
		if(s_gil[ig].modo == GSON_MODO_FULL)
		{
			//bufr[pos_bytes] = tipo2;
			s_gil[ig].pos_bytes += 1;
		}
		vezes = 1;
		/*
		if(tipo2==GSON_tSTRING)
		{
			if(cont_list_b==0)  // por mais que nao vá utilizar... para 1 string somente
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
		//memcpy(multi_data, &bufr[pos_bytes], 1*vezes);
		//pos_bytes += 1*vezes;
		break;
	case GSON_tINT16:
	case GSON_tUINT16:
		nbytes=2;
		//memcpy(multi_data, &bufr[pos_bytes], 2*vezes);
		//pos_bytes += 2*vezes;
		break;
	case GSON_tINT32:
	case GSON_tUINT32:
		nbytes=4;
		//memcpy(multi_data, &bufr[pos_bytes], 4*vezes);
		//pos_bytes += 4*vezes;
		break;
	case GSON_tINT64:
	case GSON_tUINT64:
		nbytes=8;
		//memcpy(multi_data, &bufr[pos_bytes], 8*vezes);
		//pos_bytes += 8*vezes;
		break;
	case GSON_tFLOAT32:
		nbytes=4;
		//memcpy(multi_data, &bufr[pos_bytes], 4*vezes);
		//pos_bytes += 4*vezes;
		break;
	case GSON_tFLOAT64:
		nbytes=8;
		//memcpy(multi_data, &bufr[pos_bytes], 8*vezes);
		//pos_bytes += 8*vezes;
		break;
	case GSON_tSTRING:
		// só vai...
		break;
	default:
		erro = -25;
		goto deu_erro;
	}

	if(tipo2 == GSON_tSTRING)
	{
		for(i=0; i<vezes; i++)
		{
			len = s_gil[ig].bufr[s_gil[ig].pos_bytes];
			s_gil[ig].pos_bytes += 1;
			/*
			if(vezes==1)
			{
				memcpy(multi_data, &s_gil[ig].bufr[s_gil[ig].pos_bytes], len);
			}
			else
			{
				memcpy(multi_data+(i*cont_list_b), &s_gil[ig].bufr[s_gil[ig].pos_bytes], len);
			}
			*/
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

	if(erro==0)
	{
		s_gil[ig].cont_itens += 1;
	}
	else
	{
#if (USO_DEBUG_LIB==1)
		printf("DEBUG gilson_decode_data::: ig:%u, ERRO:%i modo:%u, tipo1:%u, tipo2:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u\n", ig, erro, s_gil[ig].modo, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step);
#endif
	}

	return erro;
}



// só funciona para modo 'GSON_MODO_FULL'!!!!
int32_t gilson_decode_data_full(const uint8_t chave, uint8_t *valor)
{
	int32_t erro=0, len;
	uint16_t i, j, init, vezes = 1, cont_list_a=0, cont_list_b=0, cont_list_step=0, nbytes=1, temp16;
	uint8_t tipo1=255, tipo2=255, bypass=0;

	if(s_gil[ig].ativo==0)
	{
		erro = -26;
		goto deu_erro;
	}

	// somente funciona no modo 'GSON_MODO_FULL'!!!!
	if(s_gil[ig].modo != GSON_MODO_FULL)
	{
		erro = -27;
		goto deu_erro;
	}

	if(chave >= s_gil[ig].cont_itens2)
	{
		// quer add uma chave maior do que a contagem total que é decodificada em decode_init e aloca em 'cont_itens2'
		// nesse caso nao tem como prosseguir!!!!
		erro = -6;
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



		if(s_gil[ig].bufr[s_gil[ig].pos_bytes] == GSON_tLIST)
		{
			tipo1 = GSON_LIST;
			s_gil[ig].pos_bytes += 1;
			tipo2 = s_gil[ig].bufr[s_gil[ig].pos_bytes];
			s_gil[ig].pos_bytes += 1;
		}
		else if(s_gil[ig].bufr[s_gil[ig].pos_bytes] == GSON_tMTX2D)
		{
			tipo1 = GSON_MTX2D;
			s_gil[ig].pos_bytes += 1;
			tipo2 = s_gil[ig].bufr[s_gil[ig].pos_bytes];
			s_gil[ig].pos_bytes += 1;
		}
		else
		{
			tipo1 = GSON_SINGLE;
			tipo2 = s_gil[ig].bufr[s_gil[ig].pos_bytes];
			s_gil[ig].pos_bytes += 1;
		}

		if(tipo1>=GSON_MAX || tipo2>=GSON_tMAX)
		{
			erro = -28;
			goto deu_erro;
		}

		if(tipo1 == GSON_LIST)
		{
			memcpy(&vezes, &s_gil[ig].bufr[s_gil[ig].pos_bytes], 2);
			s_gil[ig].pos_bytes += 2;

			if(vezes == 0)
			{
				erro = -29;
				goto deu_erro;
			}

			if(tipo2 == GSON_tSTRING)
			{
				cont_list_b = s_gil[ig].bufr[s_gil[ig].pos_bytes];
				s_gil[ig].pos_bytes += 1;

				if(cont_list_b==0)
				{
					// precisamos do offset cru da lista de strings...
					erro = -30;
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
				erro = -31;
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
				if(cont_list_b==0)  // por mais que nao vá utilizar... para 1 string somente
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
			//memcpy(multi_data, &bufr[pos_bytes], 1*vezes);
			//pos_bytes += 1*vezes;
			break;
		case GSON_tINT16:
		case GSON_tUINT16:
			nbytes=2;
			//memcpy(multi_data, &bufr[pos_bytes], 2*vezes);
			//pos_bytes += 2*vezes;
			break;
		case GSON_tINT32:
		case GSON_tUINT32:
			nbytes=4;
			//memcpy(multi_data, &bufr[pos_bytes], 4*vezes);
			//pos_bytes += 4*vezes;
			break;
		case GSON_tINT64:
		case GSON_tUINT64:
			nbytes=8;
			//memcpy(multi_data, &bufr[pos_bytes], 8*vezes);
			//pos_bytes += 8*vezes;
			break;
		case GSON_tFLOAT32:
			nbytes=4;
			//memcpy(multi_data, &bufr[pos_bytes], 4*vezes);
			//pos_bytes += 4*vezes;
			break;
		case GSON_tFLOAT64:
			nbytes=8;
			//memcpy(multi_data, &bufr[pos_bytes], 8*vezes);
			//pos_bytes += 8*vezes;
			break;
		case GSON_tSTRING:
			// só vai...
			break;
		default:
			erro = -32;
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
					/*
					if(vezes==1)
					{
						memcpy(multi_data, &s_gil[ig].bufr[s_gil[ig].pos_bytes], len);
					}
					else
					{
						memcpy(multi_data+(i*cont_list_b), &s_gil[ig].bufr[s_gil[ig].pos_bytes], len);
					}
					*/
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


		deu_erro:

		if(erro==0)
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



// só funciona para modo 'GSON_MODO_FULL'!!!!
int32_t gilson_decode_data_full_old(const uint8_t chave, uint8_t *valor, const uint8_t bypass)
{
	int32_t erro=0, len;
	uint16_t i, vezes = 1, cont_list_a=0, cont_list_b=0, cont_list_step=0, nbytes=1, temp16;
	uint8_t tipo1=255, tipo2=255;

	if(s_gil[ig].ativo==0)
	{
		erro = -26;
		goto deu_erro;
	}

	// somente funciona no modo 'GSON_MODO_FULL'!!!!
	if(s_gil[ig].modo != GSON_MODO_FULL)
	{
		erro = -27;
		goto deu_erro;
	}

	if(chave >= s_gil[ig].cont_itens2)
	{
		// quer add uma chave maior do que a contagem total que é decodificada em decode_init e aloca em 'cont_itens2'
		// nesse caso nao tem como prosseguir!!!!
		erro = -6;
		goto deu_erro;
	}


	if(chave > s_gil[ig].cont_itens)
	{
		// quer add uma chave maior do que a contagem crescente...
		// vamos varrer todas as chaves até achar a 'chave' desejada mas só funciona no modo 'GSON_MODO_FULL'!!!
		erro = -6;
		goto deu_erro;
	}
	s_gil[ig].chave_atual = chave;  // salva a última feita ok
	s_gil[ig].cont_itens_old = s_gil[ig].cont_itens;

	if(s_gil[ig].bufr[s_gil[ig].pos_bytes] == GSON_tLIST)
	{
		tipo1 = GSON_LIST;
		s_gil[ig].pos_bytes += 1;
		tipo2 = s_gil[ig].bufr[s_gil[ig].pos_bytes];
		s_gil[ig].pos_bytes += 1;
	}
	else if(s_gil[ig].bufr[s_gil[ig].pos_bytes] == GSON_tMTX2D)
	{
		tipo1 = GSON_MTX2D;
		s_gil[ig].pos_bytes += 1;
		tipo2 = s_gil[ig].bufr[s_gil[ig].pos_bytes];
		s_gil[ig].pos_bytes += 1;
	}
	else
	{
		tipo1 = GSON_SINGLE;
		tipo2 = s_gil[ig].bufr[s_gil[ig].pos_bytes];
		s_gil[ig].pos_bytes += 1;
	}

	if(tipo1>=GSON_MAX || tipo2>=GSON_tMAX)
	{
		erro = -28;
		goto deu_erro;
	}

	if(tipo1 == GSON_LIST)
	{
		memcpy(&vezes, &s_gil[ig].bufr[s_gil[ig].pos_bytes], 2);
		s_gil[ig].pos_bytes += 2;

		if(vezes == 0)
		{
			erro = -29;
			goto deu_erro;
		}

		if(tipo2 == GSON_tSTRING)
		{
			cont_list_b = s_gil[ig].bufr[s_gil[ig].pos_bytes];
			s_gil[ig].pos_bytes += 1;

			if(cont_list_b==0)
			{
				// precisamos do offset cru da lista de strings...
				erro = -30;
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
			erro = -31;
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
			if(cont_list_b==0)  // por mais que nao vá utilizar... para 1 string somente
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
		//memcpy(multi_data, &bufr[pos_bytes], 1*vezes);
		//pos_bytes += 1*vezes;
		break;
	case GSON_tINT16:
	case GSON_tUINT16:
		nbytes=2;
		//memcpy(multi_data, &bufr[pos_bytes], 2*vezes);
		//pos_bytes += 2*vezes;
		break;
	case GSON_tINT32:
	case GSON_tUINT32:
		nbytes=4;
		//memcpy(multi_data, &bufr[pos_bytes], 4*vezes);
		//pos_bytes += 4*vezes;
		break;
	case GSON_tINT64:
	case GSON_tUINT64:
		nbytes=8;
		//memcpy(multi_data, &bufr[pos_bytes], 8*vezes);
		//pos_bytes += 8*vezes;
		break;
	case GSON_tFLOAT32:
		nbytes=4;
		//memcpy(multi_data, &bufr[pos_bytes], 4*vezes);
		//pos_bytes += 4*vezes;
		break;
	case GSON_tFLOAT64:
		nbytes=8;
		//memcpy(multi_data, &bufr[pos_bytes], 8*vezes);
		//pos_bytes += 8*vezes;
		break;
	case GSON_tSTRING:
		// só vai...
		break;
	default:
		erro = -32;
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
				/*
				if(vezes==1)
				{
					memcpy(multi_data, &s_gil[ig].bufr[s_gil[ig].pos_bytes], len);
				}
				else
				{
					memcpy(multi_data+(i*cont_list_b), &s_gil[ig].bufr[s_gil[ig].pos_bytes], len);
				}
				*/
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


	deu_erro:

	if(erro==0)
	{
		s_gil[ig].cont_itens += 1;
	}
	else
	{
#if (USO_DEBUG_LIB==1)
		printf("DEBUG gilson_decode_data_full::: ig:%u, ERRO:%i tipo1:%u, tipo2:%u, cont_list_a:%u, cont_list_b:%u, cont_list_step:%u\n", ig, erro, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step);
#endif
	}

	return erro;
}

