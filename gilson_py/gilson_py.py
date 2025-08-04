#!/usr/local/bin/python3
# -*- coding: utf-8 -*-

"""
DATA: 30/12/2024
    versão 0.2 do formato "gilson", com esquema de KV ala key:value, entao temos que entrar com nome da chave...
    resalvas em comparação a lib em C, aqui nao tem esquema de ponteiros/vetor em RAM e os retornos são diferentes
    foi mantido mesmo esquema de 'struct_gilson' limitando a 'LEN_PACKS_GILSON' o tamanho por mais que aqui a RAM é gigante...
    mas vai de utilizarmos em um ambiente micropython...
DATA: 18/01/25
    versão 0.3
    separado as funcoes caso vamos utilizar no estilo KV chama funcoes especificar para
DATA: 29/07/25
    ajustes com base na versão 0.53 gilson_c
"""

import struct
import numpy as np
from enum import Enum

USO_DEBUG_LIB = False  # printa ou nao mensagens de debug...

# constantes globais:
GSON_MODO_ZIP = 0  # é cru, sem nada, somente a data bruta, tem 1 byte adicional que é [0]=modo
GSON_MODO_FULL = 1  # modo padão com offset, crc, identificador
GSON_MODO_KV = 2  # modo GSON_MODO_FULL + KV ala key:value chave:valor salva nome em string das chaves ala JSON da vida
GSON_MODO_KV_ZIP = 3  # modo GSON_MODO_ZIP porem salva os nomes das chaves ala JSON da vida
GSON_MODO_MAX = 4  # indica máximo, para ser válido tem que ser menor que isso

GSON_SINGLE = 0  # valor unico
GSON_LIST = 1  # é no formato lista, [u16] mas até 64k
GSON_MTX2D = 2  # é no formato matriz, max 2 dimenções!!! [u8][u16] mas jamais pode passar de 64k!!!!
GSON_MAX = 3  # indica máximo, para ser válido tem que ser menor que isso

# reserva o '0' para outros usos...
GSON_tBIT = 1
GSON_tINT8 = 2
GSON_tUINT8 = 3
GSON_tINT16 = 4
GSON_tUINT16 = 5
GSON_tINT32 = 6
GSON_tUINT32 = 7
GSON_tINT64 = 8
GSON_tUINT64 = 9
GSON_tFLOAT32 = 10
GSON_tFLOAT64 = 11
GSON_tSTRING = 12
GSON_tMAX = 13  # indica máximo, para ser válido tem que ser menor que isso

OFFSET_MODO_ZIP = 1
OFFSET_MODO_FULL = 8

PACK_MAX_BYTES = 65536  # max 65536 bytes um pacote gilson

LEN_MAX_CHAVE_NOME = 16  # tamanho máximo do nome chave de cada elemento ala JSON quando utilizado modo 'GSON_MODO_KV'
LEN_MAX_STRING_DATA = 255  # tamanho maximo de um item que é do tipo string, seja 'GSON_SINGLE' ou 'GSON_LIST'

LEN_PACKS_GILSON = 2  # 2 pacotes manipulaveis "ao mesmo tempo", se abriu o segundo tem que fechar para voltar para o primeiro!!!

TIPO_GSON_LDIN = 0b11100000  # lista dinâmica de dados de diversos tipos, limitado até 255 tipos e nao pode ter lista de lista
TIPO_GSON_NULL = 0b11111111  # quando for entrar com uma data nula, vai chamar uma função específica para sinalizar que vai gravar a chave porem nao terá dados

erGSON_OK = 0
erGSON_0 = -1
erGSON_1 = -2
erGSON_2 = -3
erGSON_3 = -4
erGSON_4 = -5
erGSON_5 = -6
erGSON_6 = -7
erGSON_7 = -8
erGSON_8 = -9
erGSON_9 = -10
erGSON_10 = -11
erGSON_11 = -12
erGSON_12 = -13
erGSON_13 = -14
erGSON_14 = -15
erGSON_15 = -16
erGSON_16 = -17
erGSON_17 = -18
erGSON_18 = -19
erGSON_19 = -20
erGSON_20 = -21
erGSON_21 = -22
erGSON_22 = -23
erGSON_23 = -24
erGSON_24 = -25
erGSON_25 = -26
erGSON_26 = -27
erGSON_27 = -28
erGSON_28 = -29
erGSON_29 = -30
erGSON_30 = -31
erGSON_31 = -32
erGSON_32 = -33
erGSON_33 = -34
erGSON_34 = -35
erGSON_35 = -36
erGSON_STRMAX = -37
erGSON_36 = -38
erGSON_37 = -39
erGSON_38 = -40
erGSON_39 = -41
erGSON_40 = -42
erGSON_41 = -43
erGSON_42 = -44
erGSON_43 = -45
erGSON_44 = -46
erGSON_45 = -47
erGSON_46 = -48
erGSON_47 = -49
erGSON_48 = -50
erGSON_49 = -51
erGSON_50 = -52
erGSON_51 = -53
erGSON_52 = -54
erGSON_53 = -55
erGSON_54 = -56
erGSON_55 = -57
erGSON_56 = -58


class struct_gilson:
    def __init__(self):
        self.crc = 0
        self.crc_out = 0  # da leitura
        self.erro = 0  # backup de erro geral de cada operacao...
        self.pos_bytes = 0
        self.pos_bytes2 = 0
        self.size_max_pack = 0

        self.pos_tipo_dl_init = 0
        self.pos_tipo_dl_end = 0
        self.cont_tipo_dinamico = 0
        self.pos_bytes_dl = 0

        self.modo = None
        self.ativo = False
        self.cont_itens = 0
        self.cont_itens2 = 0
        self.cont_itens_old = 0
        self.chave_atual = 0  # para fins de comparar e validar chaves durante encode
        self.tipo_dinamico = 0
        self.tam_list = 0
        self.nitens = 0
        self.tam_list2 = 0
        self.nitens2 = 0
        self.chave_dl = 0
        self.chaves_null = 0  # até 255 chaves nulas, meio improvável...

        self.bufw = bytearray()  # para escrita do pacote
        self.bufr = bytearray()  # para leitura do pacote

    def clear(self):
        self.crc = 0
        self.crc_out = 0  # da leitura
        self.erro = 0  # backup de erro geral de cada operacao...
        self.pos_bytes = 0
        self.pos_bytes2 = 0
        self.size_max_pack = 0

        self.pos_tipo_dl_init = 0
        self.pos_tipo_dl_end = 0
        self.cont_tipo_dinamico = 0
        self.pos_bytes_dl = 0

        self.modo = None
        self.ativo = False
        self.cont_itens = 0
        self.cont_itens2 = 0
        self.cont_itens_old = 0
        self.chave_atual = 0
        self.tipo_dinamico = 0
        self.tam_list = 0
        self.nitens = 0
        self.tam_list2 = 0
        self.nitens2 = 0
        self.chave_dl = 0
        self.chaves_null = 0

        self.bufw.clear()
        self.bufr.clear()


def gil_crc(crc, buffer, size):
    """
    copiado da lib do littlefs "lfs_crc()"
    mesma funcao utilizada na balanza em code C
    """
    rtable = 0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c, \
        0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c

    data = buffer

    for i in range(size):
        crc = (crc >> 4) ^ rtable[(crc ^ (data[i] >> 0)) & 0xf]
        crc = (crc >> 4) ^ rtable[(crc ^ (data[i] >> 4)) & 0xf]

    crc &= 0xffffffff
    return crc


class gilson:
    def __init__(self):
        self.s_gil = [struct_gilson() for _ in range(LEN_PACKS_GILSON)]
        self.ig = 0

    def check_modo_KV(self):
        if self.s_gil[self.ig].modo == GSON_MODO_KV or self.s_gil[self.ig].modo == GSON_MODO_KV_ZIP:
            return True
        else:
            return False

    def check_modo_FULL(self):
        if self.s_gil[self.ig].modo == GSON_MODO_FULL or self.s_gil[self.ig].modo == GSON_MODO_KV:
            return True
        else:
            return False

    def check_modo_ZIP(self):
        if self.s_gil[self.ig].modo == GSON_MODO_ZIP or self.s_gil[self.ig].modo == GSON_MODO_KV_ZIP:
            return True
        else:
            return False

    def check_not_dinamico(self):
        if self.s_gil[self.ig].tipo_dinamico == 0:
            return True
        else:
            return False

    # ====================================================================================================================
    # ====================================================================================================================
    # ====================================================================================================================
    # ====================================================================================================================
    # PARTE DE CODIFICAÇÃO
    def encode_init(self, modo, size_max_pack):
        erro = erGSON_OK
        if self.s_gil[self.ig].ativo:
            erro = erGSON_0
            if self.ig == 0:
                if self.s_gil[self.ig + 1].ativo:
                    erro = erGSON_1
                else:
                    erro = erGSON_OK
                    self.ig = 1

        if erro == erGSON_OK:
            self.s_gil[self.ig].clear()  # limpa estrutura geral antes de iniciar um novo ciclo de encode
            self.s_gil[self.ig].modo = modo
            self.s_gil[self.ig].size_max_pack = size_max_pack
            if self.check_modo_FULL():
                self.s_gil[self.ig].pos_bytes = OFFSET_MODO_FULL  # offset
            else:
                self.s_gil[self.ig].pos_bytes = OFFSET_MODO_ZIP  # offset
            self.s_gil[self.ig].bufw = bytearray(self.s_gil[self.ig].pos_bytes)  # ja reserva header

            self.s_gil[self.ig].ativo = True

        if USO_DEBUG_LIB:
            print(f"DEBUG encode_init::: ig:{self.ig}, erro:{erro}, modo:{self.s_gil[self.ig].modo}, pos_bytes:{self.s_gil[self.ig].pos_bytes}, cont_itens:{self.s_gil[self.ig].cont_itens}")

        self.s_gil[self.ig].erro = erro

        return erro

    def encode_end_base(self, flag_crc=True):
        """
        // 8 bytes de offset geral:
        // [0] 1b = modo
        // [1:4] 4b = crc pacote (crc32)
        // [5:6] 2b = tamanho pacote (até 65355 bytes)
        // [7] 1b = quantos elementos geral (nao soma as listas, trata como 1) até 255 elementos/itens
        // [8::] data
        """
        erro = erGSON_OK
        if self.s_gil[self.ig].erro != erGSON_OK:
            erro = self.s_gil[self.ig].erro  # vamos manter sempre o mesmo erro!!!
            self.s_gil[self.ig].pos_bytes = 0  # pra garantir que não deu nada...
        else:
            self.s_gil[self.ig].crc = 0  # vamos calcular o crc do pacote
            self.s_gil[self.ig].bufw[0] = self.s_gil[self.ig].modo

            if self.check_modo_FULL():
                self.s_gil[self.ig].bufw[5:7] = struct.pack("H", self.s_gil[self.ig].pos_bytes)
                self.s_gil[self.ig].bufw[7] = self.s_gil[self.ig].cont_itens
                self.s_gil[self.ig].crc = gil_crc(0xffffffff, self.s_gil[self.ig].bufw[5::], self.s_gil[self.ig].pos_bytes - 5)
                self.s_gil[self.ig].bufw[1:5] = struct.pack("I", self.s_gil[self.ig].crc)
            else:
                if flag_crc:
                    self.s_gil[self.ig].crc = gil_crc(0xffffffff, self.s_gil[self.ig].bufw, self.s_gil[self.ig].pos_bytes)

        self.s_gil[self.ig].ativo = False

        if USO_DEBUG_LIB:
            print(f"DEBUG encode_end_base::: erro:{erro}, ig:{self.ig}, modo:{self.s_gil[self.ig].modo} pos_bytes:{self.s_gil[self.ig].pos_bytes}, cont_itens:{self.s_gil[self.ig].cont_itens} crc:{self.s_gil[self.ig].crc} cru:{self.s_gil[self.ig].bufw[0:8]}")

        # retorna 3 dados: total de bytes, crc e buffer cru...
        data_return = self.s_gil[self.ig].pos_bytes, self.s_gil[self.ig].crc, bytes(self.s_gil[self.ig].bufw)  # self.s_gil[self.ig].bufw[::]

        if self.ig == 1:
            self.ig = 0  # volta para o primeiro, pois está aberto!!!

        return data_return

    def encode_end_crc(self):
        return self.encode_end_base(True)

    def encode_end(self):
        return self.encode_end_base(False)

    def encode_base(self, chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step):
        erro = erGSON_OK
        vezes = 1
        nbytes = np.uint8  # padrao normalmente no C
        len_string_max = 0

        def goto_deu_erro():
            self.s_gil[self.ig].erro = erro
            if USO_DEBUG_LIB:
                print(f"DEBUG gilson_set_data::: ig:{self.ig}, ERRO:{erro}, modo:{self.s_gil[self.ig].modo}, tipo1:{tipo1}, tipo2:{tipo2}, cont_list_a:{cont_list_a}, cont_list_b:{cont_list_b}, cont_list_step:{cont_list_step}")
            return erro

        if self.s_gil[self.ig].erro != erGSON_OK:
            erro = self.s_gil[self.ig].erro  # vamos manter sempre o mesmo erro!!!
            goto_deu_erro()

        if not self.s_gil[self.ig].ativo:
            erro = erGSON_2
            goto_deu_erro()

        if tipo1 != 255 and tipo2 != 255:
            if tipo1 >= GSON_MAX or tipo2 >= GSON_tMAX:
                erro = erGSON_3
                goto_deu_erro()
        else:
            if self.check_modo_ZIP():
                # esquema de nulo nao aceita em modo ZIP!!
                erro = erGSON_36
                goto_deu_erro()

        if chave > self.s_gil[self.ig].cont_itens:
            # quer add uma chave maior do que a contagem crescente... nao tem como
            erro = erGSON_4
            goto_deu_erro()

        """
        # --------------------------------------------------------------------
        # varifica quantos bytes vai precisar para essa chave...
        check = gilson_encode_pack_chave_len(nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step)
        if check<0 or self.s_gil[self.ig].pos_bytes + check > self.s_gil[self.ig].size_max_pack:
            erro = -4
            goto_deu_erro()
        # --------------------------------------------------------------------
        """

        if self.check_modo_KV():
            nome_chave_b = nome_chave.encode(encoding='utf-8')
            len_chave = len(nome_chave_b)
            if len_chave > LEN_MAX_CHAVE_NOME:
                erro = erGSON_5
                goto_deu_erro()
            else:
                if self.check_not_dinamico():
                    b = bytearray(len_chave + 1)
                    b[0] = len_chave
                    b[1::] = nome_chave_b
                    self.s_gil[self.ig].pos_bytes += (len_chave + 1)
                    self.s_gil[self.ig].bufw += b

        # 0baaabbbbb = a:tipo1, b=tipo2
        tipo_mux = tipo1 << 5
        tipo_mux |= tipo2

        if self.check_modo_FULL() and self.check_not_dinamico():
            b = bytearray(1)
            b[0] = tipo_mux
            self.s_gil[self.ig].pos_bytes += 1
            self.s_gil[self.ig].bufw += b

        if tipo_mux != TIPO_GSON_NULL:
            if tipo1 == GSON_LIST:
                if cont_list_a == 0:
                    erro = erGSON_6
                    goto_deu_erro()
                vezes = cont_list_a

                if self.check_modo_FULL() and self.check_not_dinamico():
                    b = bytearray(2)
                    b[0:2] = struct.pack("H", vezes)
                    self.s_gil[self.ig].pos_bytes += 2
                    self.s_gil[self.ig].bufw += b

                if tipo2 == GSON_tSTRING:
                    # aqui 'cont_list_b' é tratado como uint8
                    if cont_list_b == 0:
                        erro = erGSON_7
                        goto_deu_erro()
                    elif cont_list_b > LEN_MAX_STRING_DATA:
                        erro = erGSON_STRMAX
                        goto_deu_erro()
                    else:
                        len_string_max = cont_list_b
                        if self.check_modo_FULL() and self.check_not_dinamico():
                            b = bytearray(1)
                            b[0] = len_string_max
                            self.s_gil[self.ig].pos_bytes += 1
                            self.s_gil[self.ig].bufw += b
            elif tipo1 == GSON_MTX2D:
                if tipo2 == GSON_tSTRING:
                    # nao testado isso ainda, vai da ruim
                    erro = erGSON_32
                    goto_deu_erro()

                # aqui 'cont_list_b' é tratado como uint16
                if cont_list_a == 0 or cont_list_b == 0 or cont_list_step == 0:
                    erro = erGSON_8
                    goto_deu_erro()
                # vezes = cont_list_a * cont_list_b    nao utiliza...

                if self.check_modo_FULL() and self.check_not_dinamico():
                    b = bytearray(5)
                    b[0] = cont_list_a & 0xff
                    b[1:3] = struct.pack("H", cont_list_b)
                    b[3:5] = struct.pack("H", cont_list_step)
                    self.s_gil[self.ig].pos_bytes += 5
                    self.s_gil[self.ig].bufw += b
            else:  # GSON_SINGLE
                vezes = 1
                if tipo2 == GSON_tSTRING:
                    if cont_list_a == 0:
                        erro = erGSON_9
                        goto_deu_erro()
                    elif cont_list_a > LEN_MAX_STRING_DATA:
                        erro = erGSON_STRMAX
                        goto_deu_erro()
                    else:
                        len_string_max = cont_list_a

            # gambi python... ajuste que tudo vira 'lista'
            if vezes == 1:
                data = valor,
            else:
                data = valor

            if tipo2 == GSON_tBIT:
                ...  # nada ainda...
            elif tipo2 == GSON_tINT8:
                nbytes = np.int8
            elif tipo2 == GSON_tUINT8:
                nbytes = np.uint8
            elif tipo2 == GSON_tINT16:
                nbytes = np.int16
            elif tipo2 == GSON_tUINT16:
                nbytes = np.uint16
            elif tipo2 == GSON_tINT32:
                nbytes = np.int32
            elif tipo2 == GSON_tUINT32:
                nbytes = np.uint32
            elif tipo2 == GSON_tINT64:
                nbytes = np.int64
            elif tipo2 == GSON_tUINT64:
                nbytes = np.uint64
            elif tipo2 == GSON_tFLOAT32:
                nbytes = np.float32
            elif tipo2 == GSON_tFLOAT64:
                nbytes = np.float64
            elif tipo2 == GSON_tSTRING:
                ...  # só vai...
            else:
                erro = erGSON_10
                goto_deu_erro()

            if tipo2 == GSON_tSTRING:
                for i in range(vezes):
                    bs = data[i].encode(encoding='utf-8')
                    bs = bytearray(bs)  # para ficar manipulavel
                    lens = len(bs)
                    """
                    if lens >= len_string_max:  # pode ser que veio maior que o limite
                        lens = len_string_max  # aqui ja muda o tamanho original! melhor seria gerar um erro???
                        # vamos ver se temos um caso de utf-8 no ultimo byte ou ascii desconhecido
                        if bs[lens-1] > 0 and (bs[lens-1] < 32 or bs[lens-1] > 126):
                            bs[lens - 1] = 0  # zera ele pois senao vai bugar a string
                        ...
                        lens = len_string_max
                        bs = bs[0:lens]  # atualiza o buf da string
                        bs[lens-1] = 0
                        cont_zero = 1
                        if bs[lens-2] > 0 and (bs[lens-2] < 32 or bs[lens-2] > 126):  # tabela ascii, se caso ficou bugado em um utf-8
                            bs[lens - 2] = 0
                            cont_zero = 2
                        lens -= cont_zero
                        bs = bs[0:lens]  # atualiza o buf da string
    
                    if lens <= 255:
                        b = bytearray(lens + 1)
                        b[0] = lens
                        b[1::] = bs
                        self.s_gil[self.ig].pos_bytes += (lens + 1)
                        self.s_gil[self.ig].bufw += b
                    else:
                        erro = erGSON_11
                        goto_deu_erro()
                    """
                    if lens <= len_string_max:
                        b = bytearray(lens + 1)
                        b[0] = lens
                        b[1::] = bs
                        self.s_gil[self.ig].pos_bytes += (lens + 1)
                        self.s_gil[self.ig].bufw += b
                    else:
                        erro = erGSON_11
                        goto_deu_erro()
            elif tipo2 == GSON_tBIT:
                ...  # nada ainda...
            else:
                # para o resto é só varer bytes
                if tipo1 == GSON_MTX2D:
                    # vamos ao exemplo:
                    # float x[10][250] e na realidade vamos usar somente x[2][3] ==== x[cont_list_a][cont_list_b] com step de 'cont_list_step'
                    # 10*250=2500 elementos serializados e como é float *4 = 10000 bytes, mas a serializacao vai ir [0][0], [0][1]...[0][249], [1][0] ... [9][249]
                    # logo se eu quero x[2][3] tenho que dar os offset com base nos valores máximos (250 elementos cada linha)
                    # vamos testar....
                    b = np.array(data, dtype=nbytes)
                    self.s_gil[self.ig].pos_bytes += b.nbytes
                    self.s_gil[self.ig].bufw += b.tobytes()
                else:
                    # 'data' ja esta ajustado para lista e sabemos que os dados validos sao 'data[0:vezes]'
                    b = np.array(data, dtype=nbytes)
                    self.s_gil[self.ig].pos_bytes += b.nbytes
                    self.s_gil[self.ig].bufw += b.tobytes()
        else:
            self.s_gil[self.ig].chaves_null += 1

        if erro == erGSON_OK:
            if self.check_not_dinamico():
                self.s_gil[self.ig].cont_itens += 1
        else:
            goto_deu_erro()

        return erro

    def encode(self, chave, tipo1, tipo2, *args):
        # args vai ser uma tupla do tamanho dinamico...  |0, GSON_SINGLE, GSON_tSTRING, CAST_GIL chapa.sensor, 16|  |13, GSON_MTX2D, GSON_tFLOAT32, CAST_GIL chapa.acc, 2, 3, 3| |13, GSON_MTX2D, GSON_tFLOAT32, "acc", CAST_GIL chapa.acc, 2, 3, 3|
        cont_list_a, cont_list_b, cont_list_step = 0, 0, 0
        i = 0
        nome_chave = ""

        if self.check_modo_KV():
            nome_chave = args[i]
            i += 1

        valor = args[i]
        i += 1

        if tipo1 == GSON_SINGLE:
            if tipo2 == GSON_tSTRING:
                cont_list_a = args[i]
                i += 1
        elif tipo1 == GSON_LIST:
            cont_list_a = args[i]
            i += 1
            if tipo2 == GSON_tSTRING:
                cont_list_b = args[i]
                i += 1
        elif tipo1 == GSON_MTX2D:
            cont_list_a = args[i]
            i += 1
            cont_list_b = args[i]
            i += 1
            cont_list_step = args[i]
            i += 1
        else:
            # erro...
            ...
        #print(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step)
        return self.encode_base(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step)

    def encode_dl_init(self, chave, tam_list, nitens):
        erro = erGSON_OK

        def goto_deu_erro():
            self.s_gil[self.ig].erro = erro
            if USO_DEBUG_LIB:
                print(f"DEBUG encode_dl_init::: ig:{self.ig}, ERRO:{erro}, modo:{self.s_gil[self.ig].modo}, chave:{chave}, tam_list:{tam_list}, nitens:{nitens}")
            return erro

        if self.s_gil[self.ig].erro != erGSON_OK:
            erro = self.s_gil[self.ig].erro  # vamos manter sempre o mesmo erro!!!
            goto_deu_erro()

        if not self.s_gil[self.ig].ativo:
            erro = erGSON_33
            goto_deu_erro()

        if self.s_gil[self.ig].tipo_dinamico == 1:
            # ja esta ativo esse modo... tem que teminar antes para iniciar um novo
            erro = erGSON_34
            goto_deu_erro()

        if chave > self.s_gil[self.ig].cont_itens:
            # quer add uma chave maior do que a contagem crescente... nao tem como
            erro = erGSON_35
            goto_deu_erro()

        # aloca nome da chave!!! ainda nao tem....
        self.s_gil[self.ig].tipo_dinamico = 1  # vamos tratar um tipo dinamico
        self.s_gil[self.ig].pos_tipo_dl_init = self.s_gil[self.ig].pos_bytes
        self.s_gil[self.ig].tam_list = tam_list
        self.s_gil[self.ig].nitens = nitens
        self.s_gil[self.ig].tam_list2 = 0
        self.s_gil[self.ig].nitens2 = 0
        self.s_gil[self.ig].cont_tipo_dinamico = 0

        if self.check_modo_FULL():
            # lembrando que no padrao normal agora viria o 'tipo_mux'
            # 0baaabbbbb = a:tipo1, b=tipo2
            b = bytearray(3)
            b[0] = TIPO_GSON_LDIN
            b[1] = tam_list
            b[2] = nitens
            self.s_gil[self.ig].pos_bytes += 3
            self.s_gil[self.ig].bufw += b

        self.s_gil[self.ig].cont_itens += 1
        return erro

    def valid_encode_dl(self, item, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step):
        erro = erGSON_OK

        def goto_deu_erro():
            self.s_gil[self.ig].erro = erro
            if USO_DEBUG_LIB:
                print(f"DEBUG valid_encode_dl::: ig:{self.ig}, ERRO:{erro}")
            return erro

        if self.s_gil[self.ig].erro != erGSON_OK:
            erro = self.s_gil[self.ig].erro  # vamos manter sempre o mesmo erro!!!
            goto_deu_erro()

        if not self.s_gil[self.ig].tipo_dinamico == 0:
            erro = erGSON_37
            goto_deu_erro()

        if tipo1 >= GSON_MAX or tipo2 >= GSON_tMAX:
            erro = erGSON_38
            goto_deu_erro()

        if item > self.s_gil[self.ig].nitens:
            # quer add uma item maior do que a contagem crescente... nao tem como
            erro = erGSON_39
            goto_deu_erro()

        if tipo1 == GSON_LIST:
            if cont_list_a == 0:
                erro = erGSON_40
                goto_deu_erro()
            if tipo2==GSON_tSTRING:
                if cont_list_b == 0:
                    erro = erGSON_41
                    goto_deu_erro()
        elif tipo1 == GSON_MTX2D:
            if tipo2 == GSON_tSTRING:
                # nao testado isso ainda, vai da ruim
                erro = erGSON_42
                goto_deu_erro()
            if cont_list_a == 0 or cont_list_b == 0 or cont_list_step == 0:
                erro = erGSON_43
                goto_deu_erro()
        else:  # GSON_SINGLE
            if tipo2 == GSON_tSTRING:
                if cont_list_a == 0:
                    erro = erGSON_44
                    goto_deu_erro()

        return erro

    def encode_dl_add(self, item, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step):
        erro = erGSON_OK

        def goto_deu_erro():
            self.s_gil[self.ig].erro = erro
            if USO_DEBUG_LIB:
                print(f"DEBUG encode_dl_add::: ig:{self.ig}, ERRO:{erro}")
            return erro

        erro = self.valid_encode_dl(item, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step)
        if erro != erGSON_OK:
            goto_deu_erro()

        # 0baaabbbbb = a:tipo1, b=tipo2
        tipo_mux = tipo1 << 5
        tipo_mux |= tipo2

        if self.check_modo_FULL():
            b = bytearray(1)
            b[0] = tipo_mux
            self.s_gil[self.ig].pos_bytes += 1
            self.s_gil[self.ig].bufw += b

        if tipo1 == GSON_LIST:
            if self.check_modo_FULL():
                b = bytearray(2)
                b[0:2] = struct.pack("H", cont_list_a)
                self.s_gil[self.ig].pos_bytes += 2
                self.s_gil[self.ig].bufw += b
            if tipo2 == GSON_tSTRING:
                # aqui 'cont_list_b' é tratado como uint8
                if self.check_modo_FULL():
                    b = bytearray(1)
                    b[0] = cont_list_b
                    self.s_gil[self.ig].pos_bytes += 1
                    self.s_gil[self.ig].bufw += b
        elif tipo1 == GSON_MTX2D:
           if self.check_modo_FULL():
               b = bytearray(5)
               b[0] = cont_list_a & 0xff
               b[1:3] = struct.pack("H", cont_list_b)
               b[3:5] = struct.pack("H", cont_list_step)
               self.s_gil[self.ig].pos_bytes += 5
               self.s_gil[self.ig].bufw += b
        else:  # GSON_SINGLE
            pass

        return erro

    def encode_dl_data(self, item, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step):
        erro = erGSON_OK

        def goto_deu_erro():
            self.s_gil[self.ig].erro = erro
            if USO_DEBUG_LIB:
                print(f"DEBUG encode_dl_data::: ig:{self.ig}, ERRO:{erro}, item:{item}")
            return erro

        erro = self.valid_encode_dl(item, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step)
        if erro != erGSON_OK:
            goto_deu_erro()

        # os 2 primeiro: 'chave', 'nome_chave' nao usamos aquiii vai ser ignorados devido 's_gil[ig].tipo_dinamico'
        erro = self.encode_base(0, "x", tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step)

        if erro == erGSON_OK:
            self.s_gil[self.ig].cont_tipo_dinamico += 1
        else:
            goto_deu_erro()

        return erro

    def encode_dl_end(self):
        erro = erGSON_OK

        if self.s_gil[self.ig].tipo_dinamico == 0:
            erro = erGSON_45
        elif self.s_gil[self.ig].cont_tipo_dinamico != (self.s_gil[self.ig].tam_list * self.s_gil[self.ig].nitens):
            erro = erGSON_46

        self.s_gil[self.ig].tipo_dinamico = 0  # finalizando o tratar um tipo dinamico

        if USO_DEBUG_LIB:
            print(f"DEBUG encode_dl_end::: ig:{self.ig}, ERRO:{erro}, modo:{self.s_gil[self.ig].modo}, tam_list:{self.s_gil[self.ig].tam_list}, nitens:{self.s_gil[self.ig].nitens}, cont_tipo_dinamico:{self.s_gil[self.ig].cont_tipo_dinamico}")

        return erro

    def encode_mapfix(self, mapa, *args):
        """
        'map' pode ter até 6 'uint16_t'
        chave = map[0] (obrigatório)
        tipo1 = map[1] (obrigatório)
        tipo2 = map[2] (obrigatório)
        cont_list_a = map[3] (se for o caso...)
        cont_list_b = map[4] (se for o caso...)
        cont_list_step = map[5] (se for o caso...)
        """
        cont_list_a, cont_list_b, cont_list_step = 0, 0, 0
        i = 0
        nome_chave = ""

        if self.check_modo_KV():
            nome_chave = args[i]
            i += 1

        valor = args[i]
        i += 1

        chave = mapa[0]
        tipo1 = mapa[1]
        tipo2 = mapa[2]

        if tipo1 == GSON_SINGLE:
            if tipo2 == GSON_tSTRING:
                cont_list_a = mapa[3]
                i += 1
        elif tipo1 == GSON_LIST:
            cont_list_a = mapa[3]
            i += 1
            if tipo2 == GSON_tSTRING:
                cont_list_b = mapa[4]
                i += 1
        elif tipo1 == GSON_MTX2D:
            cont_list_a = mapa[3]
            i += 1
            cont_list_b = mapa[4]
            i += 1
            cont_list_step = mapa[5]
            i += 1
        else:
            # erro...
            ...

        #print(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step)
        return self.encode_base(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step)


    def encode_mapdin(self, mapa, *args):
        """
        'map' deve ter 3 'uint16_t'
        chave = map[0] (obrigatório)
        tipo1 = map[1] (obrigatório)
        tipo2 = map[2] (obrigatório)
        """
        cont_list_a, cont_list_b, cont_list_step = 0, 0, 0
        i = 0
        nome_chave = ""

        if self.check_modo_KV():
            nome_chave = args[i]
            i += 1

        valor = args[i]
        i += 1

        chave = mapa[0]
        tipo1 = mapa[1]
        tipo2 = mapa[2]

        if tipo1 == GSON_SINGLE:
            if tipo2 == GSON_tSTRING:
                cont_list_a = args[i]
                i += 1
        elif tipo1 == GSON_LIST:
            cont_list_a = args[i]
            i += 1
            if tipo2 == GSON_tSTRING:
                cont_list_b = args[i]
                i += 1
        elif tipo1 == GSON_MTX2D:
            cont_list_a = args[i]
            i += 1
            cont_list_b = args[i]
            i += 1
            cont_list_step = args[i]
            i += 1
        else:
            # erro...
            ...

        #print(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step)
        return self.encode_base(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step)


    def encode_data_null(self, chave, *args):
        i = 0
        nome_chave = ""

        if self.check_modo_KV():
            nome_chave = args[i]
            i += 1

        return self.encode_base(chave, nome_chave, 255, 255, 0, 0, 0, 0)

    # ====================================================================================================================
    # ====================================================================================================================
    # ====================================================================================================================
    # ====================================================================================================================
    # PARTE DE DECODIFICAÇÃO

    def decode_init(self, data_cru):
        erro = erGSON_OK
        crc1, crc2 = 0, 0
        modo = 255

        if self.s_gil[self.ig].ativo:
            erro = erGSON_12
            if self.ig == 0:
                if self.s_gil[self.ig + 1].ativo:
                    erro = erGSON_13
                else:
                    erro = erGSON_OK
                    self.ig = 1

        if erro == erGSON_OK:
            self.s_gil[self.ig].clear()  # limpa estrutura geral antes de iniciar um novo ciclo de decode
            self.s_gil[self.ig].bufr = data_cru
            self.s_gil[self.ig].modo = self.s_gil[self.ig].bufr[0]

            if self.s_gil[self.ig].modo < GSON_MODO_MAX:
                if self.check_modo_FULL():
                    crc1 = struct.unpack("I", self.s_gil[self.ig].bufr[1:5])[0]
                    self.s_gil[self.ig].pos_bytes2 = struct.unpack("H", self.s_gil[self.ig].bufr[5:7])[0]
                    self.s_gil[self.ig].cont_itens2 = self.s_gil[self.ig].bufr[7]
                    crc2 = gil_crc(0xffffffff, self.s_gil[self.ig].bufr[5::], self.s_gil[self.ig].pos_bytes2 - 5)
                    if crc1 == crc2:
                        self.s_gil[self.ig].crc_out = crc1
                        self.s_gil[self.ig].pos_bytes = OFFSET_MODO_FULL
                    else:
                        erro = erGSON_14
                else:
                    self.s_gil[self.ig].pos_bytes = OFFSET_MODO_ZIP
            else:
                erro = erGSON_15

        if erro == erGSON_OK:
            modo = self.s_gil[self.ig].modo
            self.s_gil[self.ig].ativo = True
        else:
            self.s_gil[self.ig].ativo = False

        self.s_gil[self.ig].erro = erro

        if USO_DEBUG_LIB:
            print(f"DEBUG gilson_get_init::: ig:{self.ig}, erro:{erro},  modo:{self.s_gil[self.ig].modo},  pos_bytes:{self.s_gil[self.ig].pos_bytes},  cont_itens:{self.s_gil[self.ig].cont_itens}, cru:{self.s_gil[self.ig].bufr[0:8]}, crc:{crc1}=={crc2},  pos_bytes2:{self.s_gil[self.ig].pos_bytes2}  cont_itens2:{self.s_gil[self.ig].cont_itens2}")

        return erro, modo

    def decode_valid(self, data_cru):
        erro = self.decode_init(data_cru)
        self.s_gil[self.ig].clear()
        return erro

    def decode_end_base(self, flag_crc=True):
        erro = erGSON_OK

        if self.s_gil[self.ig].erro == erGSON_OK:
            if self.check_modo_FULL():
                # self.s_gil[self.ig].crc_out ja foi calculado em 'gilson_decode_init()'
                if self.s_gil[self.ig].pos_bytes != self.s_gil[self.ig].pos_bytes2 and self.s_gil[self.ig].cont_itens != self.s_gil[self.ig].cont_itens2:
                    # chegou até o fim 'cont_itens2' mas o 'pos_bytes' nao bateu com o 'pos_bytes2', dai deu ruim
                    erro = erGSON_16
                else:
                    # indica que nao leu todas chaves ou nao foi até a última ou nao veu em ordem até o fim, mas está tudo bem
                    self.s_gil[self.ig].pos_bytes = self.s_gil[self.ig].pos_bytes2
                    self.s_gil[self.ig].cont_itens = self.s_gil[self.ig].cont_itens2
            else:
                if flag_crc:
                    self.s_gil[self.ig].crc_out = gil_crc(0xffffffff, self.s_gil[self.ig].bufr, self.s_gil[self.ig].pos_bytes)
                else:
                    self.s_gil[self.ig].crc_out = 0
            self.s_gil[self.ig].ativo = False
        else:
            erro = self.s_gil[self.ig].erro  # vamos manter sempre o mesmo erro!!!

        if USO_DEBUG_LIB:
            print(f"DEBUG gilson_get_end::: ig:{self.ig}, erro:{erro}, modo:{self.s_gil[self.ig].modo}, pos_bytes:{self.s_gil[self.ig].pos_bytes}=={self.s_gil[self.ig].pos_bytes2}, cont_itens:{self.s_gil[self.ig].cont_itens}=={self.s_gil[self.ig].cont_itens2}, crc_out:{self.s_gil[self.ig].crc_out}")

        # retorna: erro, total de bytes, crc e buffer cru...
        data_return = erro, self.s_gil[self.ig].pos_bytes, self.s_gil[self.ig].crc_out, bytes(self.s_gil[self.ig].bufr)

        if erro == erGSON_OK:
            if self.ig == 1:
                self.ig = 0
        else:
            self.s_gil[self.ig].erro = erro

        return data_return

    def decode_end_crc(self):
        return self.decode_end_base(True)

    def decode_end(self):
        return self.decode_end_base(False)

    def decode_base(self, chave, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step):
        erro = erGSON_OK
        vezes = 1
        nbytes = np.uint8  # padrao normalmente no C
        nbytes2 = 1
        multi_data = None
        nome_chave = f"{chave}"  # ja assumo como padrão caso nao utilize

        def retorna():
            self.s_gil[self.ig].erro = erro
            if erro != 0:
                if USO_DEBUG_LIB:
                    print(f"DEBUG gilson_get_data::: ig:{self.ig}, ERRO:{erro} modo:{self.s_gil[self.ig].modo}, tipo1:{tipo1}, tipo2:{tipo2}, cont_list_a:{cont_list_a}, cont_list_b:{cont_list_b}, cont_list_step:{cont_list_step}")
            if self.check_modo_KV():
                ret = erro, nome_chave, multi_data
            else:
                ret = erro, multi_data
            return ret

        if self.s_gil[self.ig].erro != erGSON_OK:
            erro = self.s_gil[self.ig].erro  # vamos manter sempre o mesmo erro!!!
            retorna()

        if not self.s_gil[self.ig].ativo:
            erro = erGSON_17
            retorna()

        if tipo1 >= GSON_MAX or tipo2 >= GSON_tMAX:
            erro = erGSON_18
            retorna()

        if chave > self.s_gil[self.ig].cont_itens:
            # quer add uma chave maior do que a contagem crescente...
            # vamos varrer todas as chaves até achar a 'chave' desejada mas só funciona no modo 'GSON_MODO_FULL'!!!
            erro = erGSON_19
            retorna()

        if self.check_modo_KV():
            # nome da chave
            len_chave = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
            self.s_gil[self.ig].pos_bytes += 1
            data = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes:self.s_gil[self.ig].pos_bytes + len_chave]
            self.s_gil[self.ig].pos_bytes += len_chave
            nome_chave = data.decode(encoding='utf-8')
            # len vai ser menor que 'LEN_MAX_CHAVE_NOME' caracteres...

        if self.check_modo_FULL():
            # tipo_mux = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes] == tipo_mux
            self.s_gil[self.ig].pos_bytes += 1

        if tipo1 == GSON_LIST:
            if cont_list_a == 0:
                erro = erGSON_20
                retorna()
            vezes = cont_list_a

            if self.check_modo_FULL():
                #vezes = struct.unpack("H", self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes: self.s_gil[self.ig].pos_bytes + 2])[0]
                self.s_gil[self.ig].pos_bytes += 2

            if tipo2 == GSON_tSTRING:
                if cont_list_b == 0:
                    erro = erGSON_21
                    retorna()
                else:
                    if self.check_modo_FULL():
                        # cont_list_b = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
                        self.s_gil[self.ig].pos_bytes += 1
        elif tipo1 == GSON_MTX2D:
            if cont_list_a == 0 or cont_list_b == 0 or cont_list_step == 0:
                erro = erGSON_22
                retorna()
            else:
                if self.check_modo_FULL():
                    # cont_list_a = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
                    self.s_gil[self.ig].pos_bytes += 1
                    # cont_list_b = struct.unpack("H", self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes: self.s_gil[self.ig].pos_bytes + 2])[0]
                    self.s_gil[self.ig].pos_bytes += 2
                    # cont_list_step = struct.unpack("H", self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes: self.s_gil[self.ig].pos_bytes + 2])[0]
                    self.s_gil[self.ig].pos_bytes += 2
            vezes = cont_list_a * cont_list_b
        else:  # GSON_SINGLE
            vezes = 1

        if tipo2 == GSON_tBIT:
            ...  # nada ainda...
        elif tipo2 == GSON_tINT8:
            nbytes = np.int8
            nbytes2 = 1
        elif tipo2 == GSON_tUINT8:
            nbytes = np.uint8
            nbytes2 = 1
        elif tipo2 == GSON_tINT16:
            nbytes = np.int16
            nbytes2 = 2
        elif tipo2 == GSON_tUINT16:
            nbytes = np.uint16
            nbytes2 = 2
        elif tipo2 == GSON_tINT32:
            nbytes = np.int32
            nbytes2 = 4
        elif tipo2 == GSON_tUINT32:
            nbytes = np.uint32
            nbytes2 = 4
        elif tipo2 == GSON_tINT64:
            nbytes = np.int64
            nbytes2 = 8
        elif tipo2 == GSON_tUINT64:
            nbytes = np.uint64
            nbytes2 = 8
        elif tipo2 == GSON_tFLOAT32:
            nbytes = np.float32
            nbytes2 = 4
        elif tipo2 == GSON_tFLOAT64:
            nbytes = np.float64
            nbytes2 = 8
        elif tipo2 == GSON_tSTRING:
            ...  # só vai...
        else:
            erro = erGSON_23
            retorna()

        if tipo2 == GSON_tSTRING:
            multi_data = []
            for i in range(vezes):
                lens = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
                self.s_gil[self.ig].pos_bytes += 1
                data = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes:self.s_gil[self.ig].pos_bytes + lens]
                self.s_gil[self.ig].pos_bytes += lens
                multi_data.append(data.decode(encoding='utf-8'))
        elif tipo2 == GSON_tBIT:
            ...  # nada ainda...
        else:
            # para o resto é só varer bytes
            if tipo1 == GSON_MTX2D:
                lenb = vezes * nbytes2
                b = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes:self.s_gil[self.ig].pos_bytes + lenb]
                self.s_gil[self.ig].pos_bytes += lenb
                data = np.frombuffer(b, dtype=nbytes)
                data = data.reshape((cont_list_a, cont_list_b))
                multi_data = data.tolist()
            else:
                lenb = vezes * nbytes2
                b = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes:self.s_gil[self.ig].pos_bytes + lenb]
                self.s_gil[self.ig].pos_bytes += lenb
                data = np.frombuffer(b, dtype=nbytes)
                multi_data = data.tolist()

        if erro == erGSON_OK:
            self.s_gil[self.ig].cont_itens += 1
            if vezes == 1:
                multi_data = multi_data[0]

        return retorna()

    def decode_base_full(self, chave):
        erro = erGSON_OK
        nbytes = np.uint8  # padrao normalmente no C
        nbytes2 = 1
        multi_data = None
        vezes = 1
        bypass = 0
        tipo_mux, tipo1, tipo2 = 0, 255, 255
        cont_list_a, cont_list_b, cont_list_step = 0, 0, 0
        nome_chave = f"{chave}"  # ja assumo como padrão caso nao utilize

        #print("decode_base_full...")

        def retorna():
            self.s_gil[self.ig].erro = erro
            if erro != erGSON_OK:
                if USO_DEBUG_LIB:
                    print(f"DEBUG gilson_get_data::: ig:{self.ig}, ERRO:{erro} modo:{self.s_gil[self.ig].modo}, tipo1:{tipo1}, tipo2:{tipo2}, cont_list_a:{cont_list_a}, cont_list_b:{cont_list_b}, cont_list_step:{cont_list_step}")
            if self.check_modo_KV():
                ret = erro, nome_chave, multi_data
            else:
                ret = erro, multi_data
            return ret

        if self.s_gil[self.ig].erro != erGSON_OK:
            erro = self.s_gil[self.ig].erro  # vamos manter sempre o mesmo erro!!!
            retorna()

        if not self.s_gil[self.ig].ativo:
            erro = erGSON_24
            retorna()

        if not self.check_modo_FULL():
            erro = erGSON_25
            retorna()

        if chave > self.s_gil[self.ig].cont_itens2:
            # quer add uma chave maior do que a contagem crescente...
            # vamos varrer todas as chaves até achar a 'chave' desejada mas só funciona no modo 'GSON_MODO_FULL'!!!
            erro = erGSON_26
            retorna()
        cont_loop_erro = 0
        while True:
            if chave > self.s_gil[self.ig].cont_itens:
                # quer ler uma chave maior do que a contagem crescente... temos que ir para frente...
                # vai iniciar de onde parou até achar o que queremos
                bypass = 1
            elif chave < self.s_gil[self.ig].chave_atual:
                # quer ler uma chave menor, que ja foi lida e/u passada... temos que ir para trás
                # vai partir de zero e vai até achar o que queremos
                bypass = 1
                self.s_gil[self.ig].cont_itens = 0
                self.s_gil[self.ig].pos_bytes = OFFSET_MODO_FULL
            else:
                bypass = 0  # é a que estamos, está na sequencia crescente correta
                self.s_gil[self.ig].chave_atual = chave  # salva a última feita ok
                self.s_gil[self.ig].cont_itens_old = self.s_gil[self.ig].cont_itens

            if self.check_modo_KV():
                # nome da chave
                len_chave = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
                self.s_gil[self.ig].pos_bytes += 1
                data = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes:self.s_gil[self.ig].pos_bytes + len_chave]
                self.s_gil[self.ig].pos_bytes += len_chave
                nome_chave = data.decode(encoding='utf-8')
                # len vai ser menor que 'LEN_MAX_CHAVE_NOME' caracteres...

            tipo_mux = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
            self.s_gil[self.ig].pos_bytes += 1

            tipo1 = tipo_mux >> 5
            tipo2 = tipo_mux & 0b11111

            if tipo1 >= GSON_MAX or tipo2 >= GSON_tMAX:
                erro = erGSON_27
                retorna()

            if tipo1 == GSON_LIST:
                vezes = struct.unpack("H", self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes: self.s_gil[self.ig].pos_bytes + 2])[0]
                self.s_gil[self.ig].pos_bytes += 2

                if vezes == 0:
                    erro = erGSON_28
                    retorna()

                if tipo2 == GSON_tSTRING:
                    cont_list_b = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
                    self.s_gil[self.ig].pos_bytes += 1

                    if cont_list_b == 0:
                        erro = erGSON_29
                        retorna()
            elif tipo1 == GSON_MTX2D:
                cont_list_a = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
                self.s_gil[self.ig].pos_bytes += 1
                cont_list_b = struct.unpack("H", self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes: self.s_gil[self.ig].pos_bytes + 2])[0]
                self.s_gil[self.ig].pos_bytes += 2
                cont_list_step = struct.unpack("H", self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes: self.s_gil[self.ig].pos_bytes + 2])[0]
                self.s_gil[self.ig].pos_bytes += 2

                if cont_list_a == 0 or cont_list_b == 0 or cont_list_step == 0:
                    erro = erGSON_30
                    retorna()

                vezes = cont_list_a * cont_list_b  # no python fizemos assim pois salva exato o que temos e nao é baseado em 'cont_list_step' como no C
            else:
                vezes = 1
                # caso seja 'tipo2==GSON_tSTRING': 'cont_list_b' só precisa na codificacao, agora é com base no 'len' da vez

            if tipo2 == GSON_tBIT:
                ...  # nada ainda...
            elif tipo2 == GSON_tINT8:
                nbytes = np.int8
                nbytes2 = 1
            elif tipo2 == GSON_tUINT8:
                nbytes = np.uint8
                nbytes2 = 1
            elif tipo2 == GSON_tINT16:
                nbytes = np.int16
                nbytes2 = 2
            elif tipo2 == GSON_tUINT16:
                nbytes = np.uint16
                nbytes2 = 2
            elif tipo2 == GSON_tINT32:
                nbytes = np.int32
                nbytes2 = 4
            elif tipo2 == GSON_tUINT32:
                nbytes = np.uint32
                nbytes2 = 4
            elif tipo2 == GSON_tINT64:
                nbytes = np.int64
                nbytes2 = 8
            elif tipo2 == GSON_tUINT64:
                nbytes = np.uint64
                nbytes2 = 8
            elif tipo2 == GSON_tFLOAT32:
                nbytes = np.float32
                nbytes2 = 4
            elif tipo2 == GSON_tFLOAT64:
                nbytes = np.float64
                nbytes2 = 8
            elif tipo2 == GSON_tSTRING:
                ...  # só vai...
            else:
                erro = erGSON_31
                retorna()

            """
            print(f"0 decode full: modo:{self.s_gil[self.ig].modo}, chave:{chave}, nome_chave:{nome_chave}, tipo1:{tipo1}, tipo2:{tipo2}, cont_list_a:{cont_list_a}, "
                  f"cont_list_b:{cont_list_b}, cont_list_step:{cont_list_step}, nbytes:{nbytes}, nbytes2:{nbytes2}, vezes:{vezes}, bypass:{bypass}, cont:{self.s_gil[self.ig].cont_itens}, erro:{erro}")
            """

            if tipo2 == GSON_tSTRING:
                multi_data = []
                for i in range(vezes):
                    lens = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
                    self.s_gil[self.ig].pos_bytes += 1
                    data = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes:self.s_gil[self.ig].pos_bytes + lens]
                    self.s_gil[self.ig].pos_bytes += lens
                    if bypass == 0:
                        multi_data.append(data.decode(encoding='utf-8'))
            elif tipo2 == GSON_tBIT:
                ...  # nada ainda...
            else:
                # para o resto é só varer bytes
                if tipo1 == GSON_MTX2D:
                    lenb = vezes * nbytes2
                    b = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes:self.s_gil[self.ig].pos_bytes + lenb]
                    self.s_gil[self.ig].pos_bytes += lenb
                    if bypass == 0:
                        data = np.frombuffer(b, dtype=nbytes)
                        data = data.reshape((cont_list_a, cont_list_b))
                        multi_data = data.tolist()
                else:
                    lenb = vezes * nbytes2
                    b = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes:self.s_gil[self.ig].pos_bytes + lenb]
                    self.s_gil[self.ig].pos_bytes += lenb
                    if bypass == 0:
                        data = np.frombuffer(b, dtype=nbytes)
                        multi_data = data.tolist()

            if erro == erGSON_OK:
                self.s_gil[self.ig].cont_itens += 1
                if bypass == 0:
                    if vezes == 1:
                        multi_data = multi_data[0]

            """
            print(f"1 decode full: modo:{self.s_gil[self.ig].modo}, chave:{chave}, nome_chave:{nome_chave}, tipo1:{tipo1}, tipo2:{tipo2}, cont_list_a:{cont_list_a}, "
                  f"cont_list_b:{cont_list_b}, cont_list_step:{cont_list_step}, nbytes:{nbytes}, nbytes2:{nbytes2}, vezes:{vezes}, bypass:{bypass}, cont:{self.s_gil[self.ig].cont_itens}, erro:{erro}")
            """

            if bypass == 0:
                break

            cont_loop_erro += 1
            if cont_loop_erro > self.s_gil[self.ig].cont_itens2:
                print("q q eh isso tcheeee!")
                break

        return retorna()

    def decode(self, chave, *args):
        # args vai ser uma tupla do tamanho dinamico...  |0, GSON_SINGLE, GSON_tSTRING, CAST_GIL chapa.sensor, 16|  |13, GSON_MTX2D, GSON_tFLOAT32, CAST_GIL chapa.acc, 2, 3, 3| |13, GSON_MTX2D, GSON_tFLOAT32, "acc", CAST_GIL chapa.acc, 2, 3, 3|
        cont_list_a, cont_list_b, cont_list_step = 0, 0, 0
        tipo1 = 255
        tipo2 = 255
        modofull = 0
        i = 0

        if self.check_modo_FULL():
            modofull = 1

        # 'nome_chave' nao vem por aqui... ele vai ser retornado no return junto com outros dados
        # 'valor' nao vem por aqui... ele vai ser retornado no return junto com outros dados

        if modofull == 0:
            tipo1 = args[i]
            i += 1
            tipo2 = args[i]
            i += 1

            if tipo1 == GSON_SINGLE:
                if tipo2 == GSON_tSTRING:
                    cont_list_a = args[i]
                    i += 1
            elif tipo1 == GSON_LIST:
                cont_list_a = args[i]
                i += 1
                if tipo2 == GSON_tSTRING:
                    cont_list_b = args[i]
                    i += 1
            elif tipo1 == GSON_MTX2D:
                cont_list_a = args[i]
                i += 1
                cont_list_b = args[i]
                i += 1
                cont_list_step = args[i]
                i += 1
            else:
                # erro...
                ...

            return self.decode_base(chave, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step)
        else:
            return self.decode_base_full(chave)

    def decode_key(self, pack, chave):
        erro = erGSON_OK
        erro, modo = self.decode_init(pack)
        if erro == 0 and modo == GSON_MODO_FULL:
            erro = self.decode(chave)
            ret = self.decode_end()  # erro, total de bytes, crc e buffer cru...
            if erro == 0:
                erro = ret[0]

        if USO_DEBUG_LIB:
            print(f"DEBUG gilson_get_data::: ig:{self.ig}, ERRO:{erro} modo:{self.s_gil[self.ig].modo}|{modo}, chave:{chave}")

        return erro

    def decode_dl_init(self, chave):
        # OBS: nao foi testado ainda em modo KV
        erro = erGSON_OK

        def retorna():
            self.s_gil[self.ig].erro = erro
            if erro != erGSON_OK:
                if USO_DEBUG_LIB:
                    print(f"DEBUG decode_dl_init::: ig:{self.ig}, ERRO:{erro} modo:{self.s_gil[self.ig].modo}, chave:{chave}, tam_list:{self.s_gil[self.ig].tam_list}, nitens:{self.s_gil[self.ig].nitens}")
            if self.check_modo_KV():
                ret = erro
            else:
                ret = erro
            return ret

        if self.s_gil[self.ig].erro != erGSON_OK:
            erro = self.s_gil[self.ig].erro  # vamos manter sempre o mesmo erro!!!
            retorna()

        if self.s_gil[self.ig].tipo_dinamico == 1:
            erro = 0
            retorna()

        if chave > self.s_gil[self.ig].cont_itens2:
            erro = 0
            retorna()

        if self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes] == TIPO_GSON_LDIN:
            self.s_gil[self.ig].tipo_dinamico = 1

            self.s_gil[self.ig].pos_bytes += 1

            self.s_gil[self.ig].tam_list = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
            self.s_gil[self.ig].pos_bytes += 1

            self.s_gil[self.ig].nitens = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
            self.s_gil[self.ig].pos_bytes += 1

            self.s_gil[self.ig].pos_tipo_dl_init = self.s_gil[self.ig].pos_bytes  # offset onde inicia os tipos de dados que vamos decodificar automaticamente
            self.s_gil[self.ig].cont_tipo_dinamico = 0
            self.s_gil[self.ig].tam_list2 = 0
            self.s_gil[self.ig].nitens2 = 0
            self.s_gil[self.ig].chave_dl = chave
        else:
            erro = 0
            retorna()

        # ... continuaa....


    def decode_dl_data(self, item):
        ...

    def decode_dl_end(self):
        ...

