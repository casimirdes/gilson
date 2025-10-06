#!/usr/local/bin/python3
# -*- coding: utf-8 -*-

"""
 ============================================================================
 Name			: gilson_py
 Author			: matheus j. mella
 Version		: 0.54
 Date			: 04/10/25
 Description 	: biblioteca 'gilson'
 GitHub			: https://github.com/casimirdes/gilson
 ============================================================================

DATA: 30/12/2024
    versão 0.2 do formato "gilson", com esquema de KV ala key:value, entao temos que entrar com nome da chave...
    resalvas em comparação a lib em C, aqui não tem esquema de ponteiros/vetor em RAM e os retornos são diferentes
    foi mantido mesmo esquema de 'struct_gilson' limitando a 'LEN_PACKS_GILSON' o tamanho por mais que aqui a RAM é gigante...
    mas vai de utilizarmos em um ambiente micropython...
DATA: 18/01/25
    versão 0.3
    separado as funcoes caso vamos utilizar no estilo KV chama funcoes especificar para
DATA: 29/07/25
    ajustes com base na versão 0.53 gilson_c
DATA: 26/09/25
    ajustes com base nas mudanças da lib em C 0.54
DATA: 30/09/25
    tratamento e mapeamento de erros
    uma leve tipagem nas funções
    uso de 'dataclasses' para organizar e tipar a struct/class principal
    python >= 3.10 ????
    flag para usar lib interna 'array' ou a externa 'numpy'
DATA: 04/10/25
    separado constantes em arquivo '_defines.py'
    criado 'IntEnum' para proteção e organização
    OBS: saiu um pouco do padrão das variáveis do C
"""

FLAG_USE_NUMPY = False  # False = usa lib 'array', True = usa a lib "numpy"

if FLAG_USE_NUMPY:
    import numpy as np
else:
    import array
import struct
from enum import IntEnum
from dataclasses import dataclass, field, fields
from typing import Final

# lib interna
from ._defines import Const, Er, Modo, Tipo1, Tipo2

#USO_DEBUG_LIB = True  # printa ou não mensagens de debug...
USO_DEBUG_LIB: Final[bool] = True

# constantes globais:
# foi tudo deslocado para '_defines.py'

@dataclass(frozen=True)
class TypeMap:
    int8: object
    uint8: object
    int16: object
    uint16: object
    int32: object
    uint32: object
    int64: object
    uint64: object
    float32: object
    float64: object

if FLAG_USE_NUMPY:
    tp = TypeMap(
        int8=np.int8,
        uint8=np.uint8,
        int16=np.int16,
        uint16=np.uint16,
        int32=np.int32,
        uint32=np.uint32,
        int64=np.int64,
        uint64=np.uint64,
        float32=np.float32,
        float64=np.float64
    )
else:
    tp = TypeMap(
        int8='b',
        uint8='B',
        int16='h',
        uint16='H',
        int32='i',
        uint32='I',
        int64='q',
        uint64='Q',
        float32='f',
        float64='d'
    )


@dataclass
class StructGilson:
    crc: int = 0
    crc_out: int = 0
    erro: int = 0
    pos_bytes: int = 0
    pos_bytes2: int = 0
    size_max_pack: int = 0

    pos_tipo_dl_init: int = 0
    pos_tipo_dl_end: int = 0
    cont_tipo_dinamico: int = 0
    pos_bytes_dl: int = 0

    modo: int | None = None
    ativo: bool = False
    tipo_operacao: int = 0
    cont_itens: int = 0
    cont_itens2: int = 0
    cont_itens_old: int = 0
    chave_atual: int = 0
    tipo_dinamico: int = 0
    tam_list: int = 0
    nitens: int = 0
    tam_list2: int = 0
    nitens2: int = 0
    chave_dl: int = 0
    chaves_null: int = 0

    bufw: bytearray = field(default_factory=bytearray)
    bufr: bytearray = field(default_factory=bytearray)

    def clear(self):
        for f in fields(self):
            nome = f.name
            #tipo = f.type
            if nome in ("bufw", "bufr"):
                getattr(self, nome).clear()
            elif nome == "modo":
                setattr(self, nome, None)
            elif nome == "ativo":
                setattr(self, nome, False)
            else:
                setattr(self, nome, 0)


def gil_crc(crc: int, buffer: bytes|bytearray, size: int):
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


def data2bytes(data, tipo_data):
    """
    data: valor dinâmico
    tipo_data: depende de 'FLAG_USE_NUMPY'
    """
    if FLAG_USE_NUMPY:
        b = np.array(data, dtype=tipo_data)  # nbytes
        pos_bytes = b.nbytes
        buff = b.tobytes()
    else:
        if isinstance(data[0], list):
            data2 = [item for row in data for item in row]  # serializa a matriz em um vetor simples
        else:
            data2 = data
        b = array.array(tipo_data, data2)
        pos_bytes = len(b) * b.itemsize
        buff = b.tobytes()
    return pos_bytes, buff  # int, bytes


def bytes2data(b: bytes, tipo_data, flag_form: int = 0, cont_list_a: int = 0, cont_list_b: int = 0):
    """
    tipo_data: depende de 'FLAG_USE_NUMPY'
    """
    multi_data = None
    if FLAG_USE_NUMPY:
        if flag_form == 1:
            data = np.frombuffer(b, dtype=tipo_data)
            data = data.reshape((cont_list_a, cont_list_b))
            multi_data = data.tolist()
        else:
            data = np.frombuffer(b, dtype=tipo_data)
            multi_data = data.tolist()
    else:
        if flag_form == 1:
            data = array.array(tipo_data)
            data.frombytes(b)
            data = data.tolist()
            multi_data = [data[i * cont_list_b:(i + 1) * cont_list_b] for i in range(cont_list_a)]
        else:
            data = array.array(tipo_data)
            data.frombytes(b)
            multi_data = data.tolist()
    return multi_data


class Gilson:
    def __init__(self):
        self.s_gil = [StructGilson() for _ in range(Const.LEN_PACKS_GILSON)]
        self.ig = 0

    def CheckModeKV(self):
        if self.s_gil[self.ig].modo == Modo.KV or self.s_gil[self.ig].modo == Modo.KV_ZIP:
            return True
        else:
            return False

    def CheckModeFULL(self):
        if self.s_gil[self.ig].modo == Modo.FULL or self.s_gil[self.ig].modo == Modo.KV:
            return True
        else:
            return False

    def CheckModeZIP(self):
        if self.s_gil[self.ig].modo == Modo.ZIP or self.s_gil[self.ig].modo == Modo.KV_ZIP:
            return True
        else:
            return False

    def CheckNotDinamic(self):
        if self.s_gil[self.ig].tipo_dinamico == 0:
            return True
        else:
            return False

    # ====================================================================================================================
    # ====================================================================================================================
    # ====================================================================================================================
    # ====================================================================================================================
    # PARTE DE CODIFICAÇÃO
    def encode_init(self, modo: int, size_max_pack: int):
        erro = Er.er_OK
        if self.s_gil[self.ig].ativo:
            erro = Er.er_0
            if self.ig == 0:
                if self.s_gil[self.ig + 1].ativo:
                    erro = Er.er_1
                else:
                    erro = Er.er_OK
                    self.ig = 1

        if erro == Er.er_OK:
            self.s_gil[self.ig].clear()  # limpa estrutura geral antes de iniciar um novo ciclo de encode
            self.s_gil[self.ig].tipo_operacao = Const.e_OPER_ENCODE
            self.s_gil[self.ig].modo = modo
            self.s_gil[self.ig].size_max_pack = size_max_pack
            if self.CheckModeFULL():
                self.s_gil[self.ig].pos_bytes = Const.OFFSET_MODO_FULL  # offset
            else:
                self.s_gil[self.ig].pos_bytes = Const.OFFSET_MODO_ZIP  # offset
            self.s_gil[self.ig].bufw = bytearray(self.s_gil[self.ig].pos_bytes)  # ja reserva header

            self.s_gil[self.ig].ativo = True

        if USO_DEBUG_LIB:
            print(f"DEBUG encode_init::: ig:{self.ig}, erro:{erro}, modo:{self.s_gil[self.ig].modo}, pos_bytes:{self.s_gil[self.ig].pos_bytes}, cont_itens:{self.s_gil[self.ig].cont_itens}")

        self.s_gil[self.ig].erro = erro

        return erro

    def encode_end_base(self, flag_crc=True):
        """
        // MODO ZIP: 8 bytes de offset geral:
        // [0] 1b = modo
        // [1] 1b = quantos elementos geral (não soma as listas, trata como 1) até 255 elementos/itens
        // [2::] data

        // MODO FULL: 8 bytes de offset geral:
        // [0] 1b = modo
        // [1:4] 4b = crc pacote (crc32)
        // [5:6] 2b = tamanho pacote (até 65355 bytes)
        // [7] 1b = quantos elementos geral (não soma as listas, trata como 1) até 255 elementos/itens
        // [8::] data
        """
        erro = Er.er_OK
        if self.s_gil[self.ig].erro != Er.er_OK or self.s_gil[self.ig].tipo_operacao != Const.e_OPER_ENCODE:
            if self.s_gil[self.ig].erro == Er.er_OK:
                self.s_gil[self.ig].erro = Er.er_OPER
            erro = self.s_gil[self.ig].erro  # vamos manter sempre o mesmo erro!!!
            self.s_gil[self.ig].pos_bytes = 0  # pra garantir que não deu nada...
            self.s_gil[self.ig].modo = 255  # deu ruimmm
            self.s_gil[self.ig].bufw[0] = self.s_gil[self.ig].modo
        else:
            self.s_gil[self.ig].crc = 0  # vamos calcular o crc do pacote
            self.s_gil[self.ig].bufw[0] = self.s_gil[self.ig].modo

            if self.CheckModeFULL():
                self.s_gil[self.ig].bufw[5:7] = struct.pack("H", self.s_gil[self.ig].pos_bytes)
                self.s_gil[self.ig].bufw[7] = self.s_gil[self.ig].cont_itens
                self.s_gil[self.ig].crc = gil_crc(0xffffffff, self.s_gil[self.ig].bufw[5::], self.s_gil[self.ig].pos_bytes - 5)
                self.s_gil[self.ig].bufw[1:5] = struct.pack("I", self.s_gil[self.ig].crc)
            else:
                self.s_gil[self.ig].bufw[1] = self.s_gil[self.ig].cont_itens
                if flag_crc:
                    self.s_gil[self.ig].crc = gil_crc(0xffffffff, self.s_gil[self.ig].bufw, self.s_gil[self.ig].pos_bytes)

        self.s_gil[self.ig].ativo = False
        self.s_gil[self.ig].tipo_operacao = Const.e_OPER_NULL

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

    def encode_base(self, chave: int, nome_chave: str, tipo1: int, tipo2: int, valor, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        erro = Er.er_OK
        vezes = 1
        nbytes = tp.uint8  # padrao normalmente no C
        len_string_max = 0

        def goto_deu_erro():
            self.s_gil[self.ig].erro = erro
            # return erro
            raise ValueError(f"erro encode_base:{erro}")

        try:
            if self.s_gil[self.ig].erro != Er.er_OK:
                erro = self.s_gil[self.ig].erro  # vamos manter sempre o mesmo erro!!!
                goto_deu_erro()

            if not self.s_gil[self.ig].ativo:
                erro = Er.er_2
                goto_deu_erro()

            if self.s_gil[self.ig].tipo_operacao != Const.e_OPER_ENCODE:
                erro = Er.er_OPER
                goto_deu_erro()

            if tipo1 != 255 and tipo2 != 255:
                if tipo1 >= Tipo1.MAX or tipo2 >= Tipo2.tMAX:
                    erro = Er.er_3
                    goto_deu_erro()
            else:
                if self.CheckModeZIP():
                    # esquema de nulo não aceita em modo ZIP!!
                    erro = Er.er_36
                    goto_deu_erro()

            if chave > self.s_gil[self.ig].cont_itens:
                erro = Er.er_58
                goto_deu_erro()

            if self.s_gil[self.ig].cont_itens >= Const.LIMIT_GSON_KEYS:
                erro = Er.er_4
                goto_deu_erro()

            if self.s_gil[self.ig].cont_itens > 0 and chave <= self.s_gil[self.ig].chave_atual and self.s_gil[self.ig].tipo_dinamico==0:
                erro = Er.er_59
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

            if self.CheckModeKV():
                nome_chave_b = nome_chave.encode(encoding='utf-8')
                len_chave = len(nome_chave_b)
                if len_chave > Const.LEN_MAX_CHAVE_NOME:
                    erro = Er.er_5
                    goto_deu_erro()
                else:
                    if self.CheckNotDinamic():
                        b = bytearray(len_chave + 1)
                        b[0] = len_chave
                        b[1::] = nome_chave_b
                        self.s_gil[self.ig].pos_bytes += (len_chave + 1)
                        self.s_gil[self.ig].bufw += b

            # 0baaabbbbb = a:tipo1, b=tipo2
            tipo_mux = tipo1 << 5
            tipo_mux |= tipo2
            tipo_mux &= 0xff

            if self.CheckModeFULL() and self.CheckNotDinamic():
                b = bytearray(1)
                b[0] = tipo_mux
                self.s_gil[self.ig].pos_bytes += 1
                self.s_gil[self.ig].bufw += b

            if tipo_mux != Const.TIPO_GSON_NULL:
                if tipo1 == Tipo1.LIST:
                    if cont_list_a == 0:
                        erro = Er.er_6
                        goto_deu_erro()
                    vezes = cont_list_a

                    if self.CheckModeFULL() and self.CheckNotDinamic():
                        b = bytearray(2)
                        b[0:2] = struct.pack("H", vezes)
                        self.s_gil[self.ig].pos_bytes += 2
                        self.s_gil[self.ig].bufw += b

                    if tipo2 == Tipo2.tSTRING:
                        # aqui 'cont_list_b' é tratado como uint8
                        if cont_list_b == 0:
                            erro = Er.er_7
                            goto_deu_erro()
                        elif cont_list_b > Const.LEN_MAX_STRING_DATA:
                            erro = Er.er_STRMAX
                            goto_deu_erro()
                        else:
                            len_string_max = cont_list_b
                            if self.CheckModeFULL() and self.CheckNotDinamic():
                                b = bytearray(1)
                                b[0] = len_string_max
                                self.s_gil[self.ig].pos_bytes += 1
                                self.s_gil[self.ig].bufw += b
                elif tipo1 == Tipo1.MTX2D:
                    if tipo2 == Tipo2.tSTRING:
                        # não testado isso ainda, vai da ruim
                        erro = Er.er_32
                        goto_deu_erro()

                    # aqui 'cont_list_b' é tratado como uint16
                    if cont_list_a == 0 or cont_list_b == 0 or cont_list_step == 0:
                        erro = Er.er_8
                        goto_deu_erro()
                    vezes = cont_list_a * cont_list_b  # não utiliza...

                    if self.CheckModeFULL() and self.CheckNotDinamic():
                        b = bytearray(5)
                        b[0] = cont_list_a & 0xff
                        b[1:3] = struct.pack("H", cont_list_b)
                        b[3:5] = struct.pack("H", cont_list_step)
                        self.s_gil[self.ig].pos_bytes += 5
                        self.s_gil[self.ig].bufw += b
                else:  # Tipo1.SINGLE
                    vezes = 1
                    if tipo2 == Tipo2.tSTRING:
                        if cont_list_a == 0:
                            erro = Er.er_9
                            goto_deu_erro()
                        elif cont_list_a > Const.LEN_MAX_STRING_DATA:
                            erro = Er.er_STRMAX
                            goto_deu_erro()
                        else:
                            len_string_max = cont_list_a

                # gambi python... ajuste que tudo vira 'lista'
                if vezes == 1:
                    data = valor,
                    #print("BBBBBBBBBBBBBBBBBBBBBBBBBBBB")
                else:
                    data = valor

                if tipo2 == Tipo2.tBIT:
                    ...  # nada ainda...
                elif tipo2 == Tipo2.tINT8:
                    nbytes = tp.int8
                elif tipo2 == Tipo2.tUINT8:
                    nbytes = tp.uint8
                elif tipo2 == Tipo2.tINT16:
                    nbytes = tp.int16
                elif tipo2 == Tipo2.tUINT16:
                    nbytes = tp.uint16
                elif tipo2 == Tipo2.tINT32:
                    nbytes = tp.int32
                elif tipo2 == Tipo2.tUINT32:
                    nbytes = tp.uint32
                elif tipo2 == Tipo2.tINT64:
                    nbytes = tp.int64
                elif tipo2 == Tipo2.tUINT64:
                    nbytes = tp.uint64
                elif tipo2 == Tipo2.tFLOAT32:
                    nbytes = tp.float32
                elif tipo2 == Tipo2.tFLOAT64:
                    nbytes = tp.float64
                elif tipo2 == Tipo2.tSTRING:
                    ...  # só vai...
                else:
                    erro = Er.er_10
                    goto_deu_erro()

                if tipo2 == Tipo2.tSTRING:
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
                            erro = Er.er_11
                            goto_deu_erro()
                        """
                        if lens <= len_string_max:
                            b = bytearray(lens + 1)
                            b[0] = lens
                            b[1::] = bs
                            self.s_gil[self.ig].pos_bytes += (lens + 1)
                            self.s_gil[self.ig].bufw += b
                        else:
                            erro = Er.er_11
                            goto_deu_erro()
                elif tipo2 == Tipo2.tBIT:
                    ...  # nada ainda...
                else:
                    # para o resto é só varer bytes
                    if tipo1 == Tipo1.MTX2D:
                        # vamos ao exemplo:
                        # float x[10][250] e na realidade vamos usar somente x[2][3] ==== x[cont_list_a][cont_list_b] com step de 'cont_list_step'
                        # 10*250=2500 elementos serializados e como é float *4 = 10000 bytes, mas a serializacao vai ir [0][0], [0][1]...[0][249], [1][0] ... [9][249]
                        # logo se eu quero x[2][3] tenho que dar os offset com base nos valores máximos (250 elementos cada linha)
                        # vamos testar....
                        try:
                            """
                            b = np.array(data, dtype=nbytes)
                            self.s_gil[self.ig].pos_bytes += b.nbytes
                            self.s_gil[self.ig].bufw += b.tobytes()
                            """
                            b = data2bytes(data, nbytes)
                            self.s_gil[self.ig].pos_bytes += b[0]
                            self.s_gil[self.ig].bufw += b[1]
                        except OverflowError:
                            erro = Er.er_OVER
                            goto_deu_erro()

                    else:
                        # 'data' ja está ajustado para lista e sabemos que os dados validos sao 'data[0:vezes]'
                        try:
                            """
                            b = np.array(data, dtype=nbytes)
                            self.s_gil[self.ig].pos_bytes += b.nbytes
                            self.s_gil[self.ig].bufw += b.tobytes()
                            """
                            b = data2bytes(data, nbytes)
                            self.s_gil[self.ig].pos_bytes += b[0]
                            self.s_gil[self.ig].bufw += b[1]
                        except OverflowError:
                            erro = Er.er_OVER
                            goto_deu_erro()
            else:
                self.s_gil[self.ig].chaves_null += 1

            if erro == Er.er_OK:
                if self.CheckNotDinamic():
                    self.s_gil[self.ig].cont_itens += 1
            else:
                goto_deu_erro()
        except Exception as err:  # except ValueError:
            #print("Erro detectado:", err)
            if erro == Er.er_OK:
                erro = Er.er_DESC1
                self.s_gil[self.ig].erro = erro

        if USO_DEBUG_LIB:
            if erro != Er.er_OK:
                print(f"DEBUG encode_base::: ig:{self.ig}, ERRO:{erro}, modo:{self.s_gil[self.ig].modo}, chave:{chave}, tipo1:{tipo1}, tipo2:{tipo2}, cont_list_a:{cont_list_a}, cont_list_b:{cont_list_b}, cont_list_step:{cont_list_step}")

        return erro

    def valid_args(self):
        ...

    def encode(self, chave: int, tipo1: int, tipo2: int, *args):
        # args vai ser uma tupla do tamanho dinamico...
        cont_list_a, cont_list_b, cont_list_step = 0, 0, 0
        i = 0
        nome_chave = ""

        try:
            if self.CheckModeKV():
                nome_chave = args[i]
                i += 1

            valor = args[i]
            i += 1

            if tipo1 == Tipo1.SINGLE:
                if tipo2 == Tipo2.tSTRING:
                    cont_list_a = args[i]
                    i += 1
            elif tipo1 == Tipo1.LIST:
                cont_list_a = args[i]
                i += 1
                if tipo2 == Tipo2.tSTRING:
                    cont_list_b = args[i]
                    i += 1
            elif tipo1 == Tipo1.MTX2D:
                cont_list_a = args[i]
                i += 1
                cont_list_b = args[i]
                i += 1
                cont_list_step = args[i]
                i += 1
            else:
                # erro... vai cair dentro de 'encode_base'
                ...

            if not isinstance(cont_list_a, int) or not isinstance(cont_list_b, int) or not isinstance(cont_list_step, int) or not isinstance(nome_chave, str):
                erro = Er.er_INSTANCE1
                self.s_gil[self.ig].erro = erro
                return erro
        except Exception as err:  # except ValueError:
            #print("Erro detectado:", err)
            erro = Er.er_DESC10
            self.s_gil[self.ig].erro = erro
            return erro

        #print(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step)
        return self.encode_base(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step)

    def encode_u8(self, chave: int, valor):
        return self.encode_base(chave, "x", Tipo1.SINGLE, Tipo2.tUINT8, valor, 0, 0, 0)

    def encode_s8(self, chave: int, valor):
        return self.encode_base(chave, "x", Tipo1.SINGLE, Tipo2.tINT8, valor, 0, 0, 0)

    def encode_u16(self, chave: int, valor):
        return self.encode_base(chave, "x", Tipo1.SINGLE, Tipo2.tUINT16, valor, 0, 0, 0)

    def encode_s16(self, chave: int, valor):
        return self.encode_base(chave, "x", Tipo1.SINGLE, Tipo2.tINT16, valor, 0, 0, 0)

    def encode_u32(self, chave: int, valor):
        return self.encode_base(chave, "x", Tipo1.SINGLE, Tipo2.tUINT32, valor, 0, 0, 0)

    def encode_s32(self, chave: int, valor):
        return self.encode_base(chave, "x", Tipo1.SINGLE, Tipo2.tINT32, valor, 0, 0, 0)

    def encode_u64(self, chave: int, valor):
        return self.encode_base(chave, "x", Tipo1.SINGLE, Tipo2.tUINT64, valor, 0, 0, 0)

    def encode_s64(self, chave: int, valor):
        return self.encode_base(chave, "x", Tipo1.SINGLE, Tipo2.tINT64, valor, 0, 0, 0)

    def encode_f32(self, chave: int, valor):
        return self.encode_base(chave, "x", Tipo1.SINGLE, Tipo2.tFLOAT32, valor, 0, 0, 0)

    def encode_f64(self, chave: int, valor):
        return self.encode_base(chave, "x", Tipo1.SINGLE, Tipo2.tFLOAT64, valor, 0, 0, 0)

    def encode_str(self, chave: int, valor, cont_list_a: int):
        return self.encode_base(chave, "x", Tipo1.SINGLE, Tipo2.tSTRING, valor, cont_list_a, 0, 0)

    def encode_lu8(self, chave: int, valor, cont_list_a: int):
        return self.encode_base(chave, "x", Tipo1.LIST, Tipo2.tUINT8, valor, cont_list_a, 0, 0)

    def encode_ls8(self, chave: int, valor, cont_list_a: int):
        return self.encode_base(chave, "x", Tipo1.LIST, Tipo2.tINT8, valor, cont_list_a, 0, 0)

    def encode_lu16(self, chave: int, valor, cont_list_a: int):
        return self.encode_base(chave, "x", Tipo1.LIST, Tipo2.tUINT16, valor, cont_list_a, 0, 0)

    def encode_ls16(self, chave: int, valor, cont_list_a: int):
        return self.encode_base(chave, "x", Tipo1.LIST, Tipo2.tINT16, valor, cont_list_a, 0, 0)

    def encode_lu32(self, chave: int, valor, cont_list_a: int):
        return self.encode_base(chave, "x", Tipo1.LIST, Tipo2.tUINT32, valor, cont_list_a, 0, 0)

    def encode_ls32(self, chave: int, valor, cont_list_a: int):
        return self.encode_base(chave, "x", Tipo1.LIST, Tipo2.tINT32, valor, cont_list_a, 0, 0)

    def encode_lu64(self, chave: int, valor, cont_list_a: int):
        return self.encode_base(chave, "x", Tipo1.LIST, Tipo2.tUINT64, valor, cont_list_a, 0, 0)

    def encode_ls64(self, chave: int, valor, cont_list_a: int):
        return self.encode_base(chave, "x", Tipo1.LIST, Tipo2.tINT64, valor, cont_list_a, 0, 0)

    def encode_lf32(self, chave: int, valor, cont_list_a: int):
        return self.encode_base(chave, "x", Tipo1.LIST, Tipo2.tFLOAT32, valor, cont_list_a, 0, 0)

    def encode_lf64(self, chave: int, valor, cont_list_a: int):
        return self.encode_base(chave, "x", Tipo1.LIST, Tipo2.tFLOAT64, valor, cont_list_a, 0, 0)

    def encode_lstr(self, chave: int, valor, cont_list_a: int, cont_list_b: int):
        return self.encode_base(chave, "x", Tipo1.LIST, Tipo2.tSTRING, valor, cont_list_a, cont_list_b, 0)

    def encode_mu8(self, chave: int, valor, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.encode_base(chave, "x", Tipo1.MTX2D, Tipo2.tUINT8, valor, cont_list_a, cont_list_b, cont_list_step)

    def encode_ms8(self, chave: int, valor, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.encode_base(chave, "x", Tipo1.MTX2D, Tipo2.tINT8, valor, cont_list_a, cont_list_b, cont_list_step)

    def encode_mu16(self, chave: int, valor, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.encode_base(chave, "x", Tipo1.MTX2D, Tipo2.tUINT16, valor, cont_list_a, cont_list_b, cont_list_step)

    def encode_ms16(self, chave: int, valor, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.encode_base(chave, "x", Tipo1.MTX2D, Tipo2.tINT16, valor, cont_list_a, cont_list_b, cont_list_step)

    def encode_mu32(self, chave: int, valor, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.encode_base(chave, "x", Tipo1.MTX2D, Tipo2.tUINT32, valor, cont_list_a, cont_list_b, cont_list_step)

    def encode_ms32(self, chave: int, valor, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.encode_base(chave, "x", Tipo1.MTX2D, Tipo2.tINT32, valor, cont_list_a, cont_list_b, cont_list_step)

    def encode_mu64(self, chave: int, valor, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.encode_base(chave, "x", Tipo1.MTX2D, Tipo2.tUINT64, valor, cont_list_a, cont_list_b, cont_list_step)

    def encode_ms64(self, chave: int, valor, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.encode_base(chave, "x", Tipo1.MTX2D, Tipo2.tINT64, valor, cont_list_a, cont_list_b, cont_list_step)

    def encode_mf32(self, chave: int, valor, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.encode_base(chave, "x", Tipo1.MTX2D, Tipo2.tFLOAT32, valor, cont_list_a, cont_list_b, cont_list_step)

    def encode_mf64(self, chave: int, valor, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.encode_base(chave, "x", Tipo1.MTX2D, Tipo2.tFLOAT64, valor, cont_list_a, cont_list_b, cont_list_step)

    def encode_dl_init(self, chave: int, tam_list: int, nitens: int):
        erro = Er.er_OK

        def goto_deu_erro():
            self.s_gil[self.ig].erro = erro
            if USO_DEBUG_LIB:
                print(f"DEBUG encode_dl_init::: ig:{self.ig}, ERRO:{erro}, modo:{self.s_gil[self.ig].modo}, chave:{chave}, tam_list:{tam_list}, nitens:{nitens}")
            #return erro
            raise ValueError(f"erro encode_dl_init:{erro}")

        try:
            if self.s_gil[self.ig].erro != Er.er_OK:
                erro = self.s_gil[self.ig].erro  # vamos manter sempre o mesmo erro!!!
                goto_deu_erro()

            if not self.s_gil[self.ig].ativo:
                erro = Er.er_33
                goto_deu_erro()

            if self.s_gil[self.ig].tipo_dinamico == 1:
                # já está ativo esse modo... tem que terminar antes para iniciar um novo
                erro = Er.er_34
                goto_deu_erro()

            if chave > self.s_gil[self.ig].cont_itens:
                # quer add uma chave maior do que a contagem crescente... não tem como
                erro = Er.er_35
                goto_deu_erro()

            # aloca nome da chave!!! ainda não tem....
            self.s_gil[self.ig].tipo_operacao = Const.e_OPER_ENCODE
            self.s_gil[self.ig].tipo_dinamico = 1  # vamos tratar um tipo dinamico
            self.s_gil[self.ig].pos_tipo_dl_init = self.s_gil[self.ig].pos_bytes
            self.s_gil[self.ig].tam_list = tam_list
            self.s_gil[self.ig].nitens = nitens
            self.s_gil[self.ig].tam_list2 = 0
            self.s_gil[self.ig].nitens2 = 0
            self.s_gil[self.ig].cont_tipo_dinamico = 0

            if self.CheckModeFULL():
                # lembrando que no padrao normal agora viria o 'tipo_mux'
                # 0baaabbbbb = a:tipo1, b=tipo2
                b = bytearray(3)
                b[0] = Const.TIPO_GSON_LDIN
                b[1] = tam_list
                b[2] = nitens
                self.s_gil[self.ig].pos_bytes += 3
                self.s_gil[self.ig].bufw += b
            else:
                # muita gambi para fazer funcionar... chato pacassss, um dia quem sabe...
                erro = Er.er_63

            self.s_gil[self.ig].cont_itens += 1

        except Exception as err:  # except ValueError:
            #print("Erro detectado:", err)
            if erro == Er.er_OK:
                erro = Er.er_DESC2
                self.s_gil[self.ig].erro = erro
        return erro

    def valid_encode_dl(self, item: int, tipo1: int, tipo2: int, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        erro = Er.er_OK

        def goto_deu_erro():
            self.s_gil[self.ig].erro = erro
            if USO_DEBUG_LIB:
                print(f"DEBUG valid_encode_dl::: ig:{self.ig}, ERRO:{erro}")
            # return erro
            raise ValueError(f"erro valid_encode_dl:{erro}")

        try:
            if self.s_gil[self.ig].erro != Er.er_OK:
                erro = self.s_gil[self.ig].erro  # vamos manter sempre o mesmo erro!!!
                goto_deu_erro()

            if self.s_gil[self.ig].tipo_dinamico == 0:
                erro = Er.er_37
                goto_deu_erro()

            if self.s_gil[self.ig].tipo_operacao != Const.e_OPER_ENCODE:
                erro = Er.er_OPER
                goto_deu_erro()

            if tipo1 >= Tipo1.MAX:
                erro = Er.er_38
                goto_deu_erro()

            if tipo2 >= Tipo2.tMAX:
                erro = Er.er_38b
                goto_deu_erro()

            if item > self.s_gil[self.ig].nitens:
                # quer add uma item maior do que a contagem crescente... não tem como
                erro = Er.er_39
                goto_deu_erro()

            if tipo1 == Tipo1.LIST:
                if cont_list_a == 0:
                    erro = Er.er_40
                    goto_deu_erro()
                if tipo2==Tipo2.tSTRING:
                    if cont_list_b == 0:
                        erro = Er.er_41
                        goto_deu_erro()
            elif tipo1 == Tipo1.MTX2D:
                if tipo2 == Tipo2.tSTRING:
                    # não testado isso ainda, vai da ruim
                    erro = Er.er_42
                    goto_deu_erro()
                if cont_list_a == 0 or cont_list_b == 0 or cont_list_step == 0:
                    erro = Er.er_43
                    goto_deu_erro()
            else:  # Tipo1.SINGLE
                if tipo2 == Tipo2.tSTRING:
                    if cont_list_a == 0:
                        erro = Er.er_44
                        goto_deu_erro()

        except Exception as err:  # except ValueError:
            #print("Erro detectado:", err)
            if erro == Er.er_OK:
                erro = Er.er_DESC3
                self.s_gil[self.ig].erro = erro
        return erro

    def encode_dl_add(self, item: int, tipo1: int, tipo2: int, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        erro = Er.er_OK

        def goto_deu_erro():
            self.s_gil[self.ig].erro = erro
            if USO_DEBUG_LIB:
                print(f"DEBUG encode_dl_add::: ig:{self.ig}, ERRO:{erro}")
            # return erro
            raise ValueError(f"erro encode_dl_add:{erro}")

        try:
            erro = self.valid_encode_dl(item, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step)
            if erro != Er.er_OK:
                goto_deu_erro()

            # 0baaabbbbb = a:tipo1, b=tipo2
            tipo_mux = tipo1 << 5
            tipo_mux |= tipo2

            if self.CheckModeFULL():
                b = bytearray(1)
                b[0] = tipo_mux
                self.s_gil[self.ig].pos_bytes += 1
                self.s_gil[self.ig].bufw += b

            if tipo1 == Tipo1.LIST:
                if self.CheckModeFULL():
                    b = bytearray(2)
                    b[0:2] = struct.pack("H", cont_list_a)
                    self.s_gil[self.ig].pos_bytes += 2
                    self.s_gil[self.ig].bufw += b
                if tipo2 == Tipo2.tSTRING:
                    # aqui 'cont_list_b' é tratado como uint8
                    if self.CheckModeFULL():
                        b = bytearray(1)
                        b[0] = cont_list_b
                        self.s_gil[self.ig].pos_bytes += 1
                        self.s_gil[self.ig].bufw += b
            elif tipo1 == Tipo1.MTX2D:
               if self.CheckModeFULL():
                   b = bytearray(5)
                   b[0] = cont_list_a & 0xff
                   b[1:3] = struct.pack("H", cont_list_b)
                   b[3:5] = struct.pack("H", cont_list_step)
                   self.s_gil[self.ig].pos_bytes += 5
                   self.s_gil[self.ig].bufw += b
            else:  # Tipo1.SINGLE
                pass

        except Exception as err:  # except ValueError:
            #print("Erro detectado:", err)
            if erro == Er.er_OK:
                erro = Er.er_DESC4
                self.s_gil[self.ig].erro = erro
        return erro

    def encode_dl_data(self, item: int, tipo1: int, tipo2: int, valor, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        erro = Er.er_OK

        def goto_deu_erro():
            self.s_gil[self.ig].erro = erro
            if USO_DEBUG_LIB:
                print(f"DEBUG encode_dl_data::: ig:{self.ig}, ERRO:{erro}, item:{item}")
            # return erro
            raise ValueError(f"erro encode_dl_data:{erro}")

        try:
            erro = self.valid_encode_dl(item, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step)
            if erro != Er.er_OK:
                goto_deu_erro()

            # os 2 primeiro: 'chave', 'nome_chave' não usamos aquiii vai ser ignorados devido 's_gil[ig].tipo_dinamico'
            erro = self.encode_base(0, "x", tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step)

            if erro == Er.er_OK:
                self.s_gil[self.ig].cont_tipo_dinamico += 1
            else:
                goto_deu_erro()

        except Exception as err:  # except ValueError:
            #print("Erro detectado:", err)
            if erro == Er.er_OK:
                erro = Er.er_DESC5
                self.s_gil[self.ig].erro = erro
        return erro

    def encode_dl_end(self):
        erro = Er.er_OK

        if self.s_gil[self.ig].tipo_dinamico == 0:
            erro = Er.er_45
        elif self.s_gil[self.ig].tipo_operacao != Const.e_OPER_ENCODE:
            erro = Er.er_OPER
        elif self.s_gil[self.ig].cont_tipo_dinamico != (self.s_gil[self.ig].tam_list * self.s_gil[self.ig].nitens):
            erro = Er.er_46

        self.s_gil[self.ig].tipo_dinamico = 0  # finalizando o tratar um tipo dinamico

        if USO_DEBUG_LIB:
            print(f"DEBUG encode_dl_end::: ig:{self.ig}, ERRO:{erro}, modo:{self.s_gil[self.ig].modo}, tam_list:{self.s_gil[self.ig].tam_list}, nitens:{self.s_gil[self.ig].nitens}, cont_tipo_dinamico:{self.s_gil[self.ig].cont_tipo_dinamico}")

        return erro

    def encode_mapfix(self, mapa: list | tuple, *args):
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

        try:
            if self.CheckModeKV():
                nome_chave = args[i]
                i += 1

            valor = args[i]
            i += 1

            chave = mapa[0]
            tipo1 = mapa[1]
            tipo2 = mapa[2]

            if tipo1 == Tipo1.SINGLE:
                if tipo2 == Tipo2.tSTRING:
                    cont_list_a = mapa[3]
                    i += 1
            elif tipo1 == Tipo1.LIST:
                cont_list_a = mapa[3]
                i += 1
                if tipo2 == Tipo2.tSTRING:
                    cont_list_b = mapa[4]
                    i += 1
            elif tipo1 == Tipo1.MTX2D:
                cont_list_a = mapa[3]
                i += 1
                cont_list_b = mapa[4]
                i += 1
                cont_list_step = mapa[5]
                i += 1
            else:
                # erro...
                ...

            if not isinstance(cont_list_a, int) or not isinstance(cont_list_b, int) or not isinstance(cont_list_step, int) or not isinstance(nome_chave, str):
                erro = Er.er_INSTANCE2
                self.s_gil[self.ig].erro = erro
                return erro
        except Exception as err:  # except ValueError:
            #print("Erro detectado:", err)
            erro = Er.er_DESC11
            self.s_gil[self.ig].erro = erro
            return erro

        #print(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step)
        return self.encode_base(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step)

    def encode_mapdin(self, mapa: list | tuple, *args):
        """
        'map' deve ter 3 'uint16_t'
        chave = map[0] (obrigatório)
        tipo1 = map[1] (obrigatório)
        tipo2 = map[2] (obrigatório)
        """
        cont_list_a, cont_list_b, cont_list_step = 0, 0, 0
        i = 0
        nome_chave = ""

        try:
            if self.CheckModeKV():
                nome_chave = args[i]
                i += 1

            valor = args[i]
            i += 1

            chave = mapa[0]
            tipo1 = mapa[1]
            tipo2 = mapa[2]

            if tipo1 == Tipo1.SINGLE:
                if tipo2 == Tipo2.tSTRING:
                    cont_list_a = args[i]
                    i += 1
            elif tipo1 == Tipo1.LIST:
                cont_list_a = args[i]
                i += 1
                if tipo2 == Tipo2.tSTRING:
                    cont_list_b = args[i]
                    i += 1
            elif tipo1 == Tipo1.MTX2D:
                cont_list_a = args[i]
                i += 1
                cont_list_b = args[i]
                i += 1
                cont_list_step = args[i]
                i += 1
            else:
                # erro...
                ...

            if not isinstance(cont_list_a, int) or not isinstance(cont_list_b, int) or not isinstance(cont_list_step, int) or not isinstance(nome_chave, str):
                erro = Er.er_INSTANCE3
                self.s_gil[self.ig].erro = erro
                return erro
        except Exception as err:  # except ValueError:
            #print("Erro detectado:", err)
            erro = Er.er_DESC12
            self.s_gil[self.ig].erro = erro
            return erro

        #print(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step)
        return self.encode_base(chave, nome_chave, tipo1, tipo2, valor, cont_list_a, cont_list_b, cont_list_step)

    def encode_data_null(self, chave: int, *args):
        i = 0
        nome_chave = ""

        try:
            if self.CheckModeKV():
                nome_chave = args[i]
                i += 1

                if not isinstance(nome_chave, str):
                    erro = Er.er_INSTANCE4
                    self.s_gil[self.ig].erro = erro
                    return erro
        except Exception as err:  # except ValueError:
            #print("Erro detectado:", err)
            erro = Er.er_DESC13
            self.s_gil[self.ig].erro = erro
            return erro

        return self.encode_base(chave, nome_chave, 255, 255, 0, 0, 0, 0)

    # ====================================================================================================================
    # ====================================================================================================================
    # ====================================================================================================================
    # ====================================================================================================================
    # PARTE DE DECODIFICAÇÃO

    def decode_init(self, data_cru: bytes | bytearray):
        erro = Er.er_OK
        crc1, crc2 = 0, 0
        modo = 255

        if self.s_gil[self.ig].ativo:
            erro = Er.er_12
            if self.ig == 0:
                if self.s_gil[self.ig + 1].ativo:
                    erro = Er.er_13
                else:
                    erro = Er.er_OK
                    self.ig = 1

        if erro == Er.er_OK:
            self.s_gil[self.ig].clear()  # limpa estrutura geral antes de iniciar um novo ciclo de decode
            self.s_gil[self.ig].tipo_operacao = Const.e_OPER_DECODE
            self.s_gil[self.ig].bufr = data_cru
            self.s_gil[self.ig].modo = self.s_gil[self.ig].bufr[0]

            if self.s_gil[self.ig].modo < Modo.MAX:
                if self.CheckModeFULL():
                    crc1 = struct.unpack("I", self.s_gil[self.ig].bufr[1:5])[0]
                    self.s_gil[self.ig].pos_bytes2 = struct.unpack("H", self.s_gil[self.ig].bufr[5:7])[0]
                    self.s_gil[self.ig].cont_itens2 = self.s_gil[self.ig].bufr[7]
                    crc2 = gil_crc(0xffffffff, self.s_gil[self.ig].bufr[5::], self.s_gil[self.ig].pos_bytes2 - 5)
                    if crc1 == crc2:
                        self.s_gil[self.ig].crc_out = crc1
                        self.s_gil[self.ig].pos_bytes = Const.OFFSET_MODO_FULL
                    else:
                        erro = Er.er_14
                else:
                    self.s_gil[self.ig].cont_itens2 = self.s_gil[self.ig].bufr[1]
                    self.s_gil[self.ig].pos_bytes = Const.OFFSET_MODO_ZIP
            else:
                erro = Er.er_15

        if erro == Er.er_OK:
            modo = self.s_gil[self.ig].modo
            self.s_gil[self.ig].ativo = True
        else:
            self.s_gil[self.ig].ativo = False
            self.s_gil[self.ig].tipo_operacao = Const.e_OPER_NULL

        self.s_gil[self.ig].erro = erro

        if USO_DEBUG_LIB:
            print(f"DEBUG gilson_get_init::: ig:{self.ig}, erro:{erro},  modo:{self.s_gil[self.ig].modo},  pos_bytes:{self.s_gil[self.ig].pos_bytes},  cont_itens:{self.s_gil[self.ig].cont_itens}, cru:{self.s_gil[self.ig].bufr[0:8]}, crc:{crc1}=={crc2},  pos_bytes2:{self.s_gil[self.ig].pos_bytes2}  cont_itens2:{self.s_gil[self.ig].cont_itens2}")

        return erro, modo

    def decode_valid(self, data_cru: bytes | bytearray):
        erro = self.decode_init(data_cru)
        self.s_gil[self.ig].clear()
        return erro

    def decode_end_base(self, flag_crc=True):
        erro = Er.er_OK

        if self.s_gil[self.ig].erro == Er.er_OK:
            if self.CheckModeFULL():
                # self.s_gil[self.ig].crc_out ja foi calculado em 'gilson_decode_init()'
                if self.s_gil[self.ig].pos_bytes != self.s_gil[self.ig].pos_bytes2 and self.s_gil[self.ig].cont_itens == self.s_gil[self.ig].cont_itens2:
                    # chegou até o fim 'cont_itens2' mas o 'pos_bytes' não bateu com o 'pos_bytes2', dai deu ruim
                    erro = Er.er_16
                else:
                    # indica que não leu todas chaves ou não foi até a última ou não veu em ordem até o fim, mas está tudo bem
                    self.s_gil[self.ig].pos_bytes = self.s_gil[self.ig].pos_bytes2
                    self.s_gil[self.ig].cont_itens = self.s_gil[self.ig].cont_itens2
            else:
                if flag_crc:
                    self.s_gil[self.ig].crc_out = gil_crc(0xffffffff, self.s_gil[self.ig].bufr, self.s_gil[self.ig].pos_bytes)
                else:
                    self.s_gil[self.ig].crc_out = 0
            self.s_gil[self.ig].ativo = False
            self.s_gil[self.ig].tipo_operacao = Const.e_OPER_NULL
        elif self.s_gil[self.ig].tipo_operacao != Const.e_OPER_DECODE:
            erro = Er.er_OPER
        else:
            erro = self.s_gil[self.ig].erro  # vamos manter sempre o mesmo erro!!!

        if USO_DEBUG_LIB:
            print(f"DEBUG gilson_get_end::: ig:{self.ig}, erro:{erro}, modo:{self.s_gil[self.ig].modo}, pos_bytes:{self.s_gil[self.ig].pos_bytes}=={self.s_gil[self.ig].pos_bytes2}, cont_itens:{self.s_gil[self.ig].cont_itens}=={self.s_gil[self.ig].cont_itens2}, crc_out:{self.s_gil[self.ig].crc_out}")

        # retorna: erro, total de bytes, crc e buffer cru...
        data_return = erro, self.s_gil[self.ig].pos_bytes, self.s_gil[self.ig].crc_out, bytes(self.s_gil[self.ig].bufr)

        if erro == Er.er_OK:
            if self.ig == 1:
                self.ig = 0
        else:
            self.s_gil[self.ig].erro = erro

        return data_return

    def decode_end_crc(self):
        return self.decode_end_base(True)

    def decode_end(self):
        return self.decode_end_base(False)

    def decode_base(self, chave: int, tipo1: int, tipo2: int, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        erro = Er.er_OK
        vezes = 1
        nbytes = tp.uint8  # padrao normalmente no C
        nbytes2 = 1
        multi_data = None
        nome_chave = f"{chave}"  # ja assumo como padrão caso não utilize

        def goto_deu_erro():
            self.s_gil[self.ig].erro = erro
            if erro != 0:
                if USO_DEBUG_LIB:
                    print(f"DEBUG gilson_get_data::: ig:{self.ig}, ERRO:{erro} modo:{self.s_gil[self.ig].modo}, chave:{chave}, tipo1:{tipo1}, tipo2:{tipo2}, cont_list_a:{cont_list_a}, cont_list_b:{cont_list_b}, cont_list_step:{cont_list_step}")
            raise ValueError(f"erro encode_base:{erro}")

        try:
            if self.s_gil[self.ig].erro != Er.er_OK:
                erro = self.s_gil[self.ig].erro  # vamos manter sempre o mesmo erro!!!
                goto_deu_erro()

            if not self.s_gil[self.ig].ativo:
                erro = Er.er_17
                goto_deu_erro()

            if self.s_gil[self.ig].tipo_operacao != Const.e_OPER_DECODE:
                erro = Er.er_OPER
                goto_deu_erro()

            if tipo1 >= Tipo1.MAX:
                erro = Er.er_18
                goto_deu_erro()

            if tipo2 >= Tipo2.tMAX:
                erro = Er.er_18b
                goto_deu_erro()

            if chave > self.s_gil[self.ig].cont_itens:
                # quer add uma chave maior do que a contagem crescente...
                # vamos varrer todas as chaves até achar a 'chave' desejada mas só funciona no modo 'GSON_MODO_FULL'!!!
                erro = Er.er_19
                goto_deu_erro()

            if chave >= self.s_gil[self.ig].cont_itens2:
                # quer ler uma chave maior do que a total programado
                erro = Er.er_60
                goto_deu_erro()

            if self.s_gil[self.ig].cont_itens>0 and chave <= self.s_gil[self.ig].chave_atual and self.s_gil[self.ig].tipo_dinamico==0:
                # chave ja foi adicionada no decode
                erro = Er.er_61
                goto_deu_erro()

            if self.CheckModeKV():
                # nome da chave
                len_chave = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
                self.s_gil[self.ig].pos_bytes += 1
                data = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes:self.s_gil[self.ig].pos_bytes + len_chave]
                self.s_gil[self.ig].pos_bytes += len_chave
                nome_chave = data.decode(encoding='utf-8')
                # len vai ser menor que 'LEN_MAX_CHAVE_NOME' caracteres...

            if self.CheckModeFULL():
                # tipo_mux = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes] == tipo_mux
                self.s_gil[self.ig].pos_bytes += 1

            if tipo1 == Tipo1.LIST:
                if cont_list_a == 0:
                    erro = Er.er_20
                    goto_deu_erro()
                vezes = cont_list_a

                if self.CheckModeFULL():
                    #vezes = struct.unpack("H", self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes: self.s_gil[self.ig].pos_bytes + 2])[0]
                    self.s_gil[self.ig].pos_bytes += 2

                if tipo2 == Tipo2.tSTRING:
                    if cont_list_b == 0:
                        erro = Er.er_21
                        goto_deu_erro()
                    else:
                        if self.CheckModeFULL():
                            # cont_list_b = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
                            self.s_gil[self.ig].pos_bytes += 1
            elif tipo1 == Tipo1.MTX2D:
                if cont_list_a == 0 or cont_list_b == 0 or cont_list_step == 0:
                    erro = Er.er_22
                    goto_deu_erro()
                else:
                    if self.CheckModeFULL():
                        # cont_list_a = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
                        self.s_gil[self.ig].pos_bytes += 1
                        # cont_list_b = struct.unpack("H", self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes: self.s_gil[self.ig].pos_bytes + 2])[0]
                        self.s_gil[self.ig].pos_bytes += 2
                        # cont_list_step = struct.unpack("H", self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes: self.s_gil[self.ig].pos_bytes + 2])[0]
                        self.s_gil[self.ig].pos_bytes += 2
                vezes = cont_list_a * cont_list_b
            else:  # Tipo1.SINGLE
                vezes = 1

            if tipo2 == Tipo2.tBIT:
                ...  # nada ainda...
            elif tipo2 == Tipo2.tINT8:
                nbytes = tp.int8
                nbytes2 = 1
            elif tipo2 == Tipo2.tUINT8:
                nbytes = tp.uint8
                nbytes2 = 1
            elif tipo2 == Tipo2.tINT16:
                nbytes = tp.int16
                nbytes2 = 2
            elif tipo2 == Tipo2.tUINT16:
                nbytes = tp.uint16
                nbytes2 = 2
            elif tipo2 == Tipo2.tINT32:
                nbytes = tp.int32
                nbytes2 = 4
            elif tipo2 == Tipo2.tUINT32:
                nbytes = tp.uint32
                nbytes2 = 4
            elif tipo2 == Tipo2.tINT64:
                nbytes = tp.int64
                nbytes2 = 8
            elif tipo2 == Tipo2.tUINT64:
                nbytes = tp.uint64
                nbytes2 = 8
            elif tipo2 == Tipo2.tFLOAT32:
                nbytes = tp.float32
                nbytes2 = 4
            elif tipo2 == Tipo2.tFLOAT64:
                nbytes = tp.float64
                nbytes2 = 8
            elif tipo2 == Tipo2.tSTRING:
                ...  # só vai...
            else:
                erro = Er.er_23
                goto_deu_erro()

            if tipo2 == Tipo2.tSTRING:
                multi_data = []
                for i in range(vezes):
                    lens = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
                    self.s_gil[self.ig].pos_bytes += 1
                    data = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes:self.s_gil[self.ig].pos_bytes + lens]
                    self.s_gil[self.ig].pos_bytes += lens
                    multi_data.append(data.decode(encoding='utf-8'))
            elif tipo2 == Tipo2.tBIT:
                ...  # nada ainda...
            else:
                # para o resto é só varer bytes
                if tipo1 == Tipo1.MTX2D:
                    lenb = vezes * nbytes2
                    b = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes:self.s_gil[self.ig].pos_bytes + lenb]
                    self.s_gil[self.ig].pos_bytes += lenb
                    """
                    data = np.frombuffer(b, dtype=nbytes)
                    data = data.reshape((cont_list_a, cont_list_b))
                    multi_data = data.tolist()
                    """
                    multi_data = bytes2data(b, nbytes, 1, cont_list_a, cont_list_b)
                else:
                    lenb = vezes * nbytes2
                    b = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes:self.s_gil[self.ig].pos_bytes + lenb]
                    self.s_gil[self.ig].pos_bytes += lenb
                    """
                    data = np.frombuffer(b, dtype=nbytes)
                    multi_data = data.tolist()
                    """
                    multi_data = bytes2data(b, nbytes)

            if erro == Er.er_OK:
                self.s_gil[self.ig].cont_itens += 1
                if vezes == 1 and tipo1 == Tipo1.SINGLE:
                    multi_data = multi_data[0]
        except Exception as err:  # except ValueError:
            #print("Erro detectado:", err)
            if erro == Er.er_OK:
                erro = Er.er_DESC6
                self.s_gil[self.ig].erro = erro

        if self.CheckModeKV():
            ret = erro, nome_chave, multi_data
        else:
            ret = erro, multi_data
        return ret

    def decode_base_full(self, chave: int):
        erro = Er.er_OK
        nbytes = tp.uint8  # padrao normalmente no C
        nbytes2 = 1
        multi_data = None
        vezes = 1
        bypass = 0
        tipo_mux, tipo1, tipo2 = 0, 255, 255
        cont_list_a, cont_list_b, cont_list_step = 0, 0, 0
        nome_chave = f"{chave}"  # ja assumo como padrão caso não utilize

        #print("decode_base_full...")

        def goto_deu_erro():
            self.s_gil[self.ig].erro = erro
            if erro != Er.er_OK:
                if USO_DEBUG_LIB:
                    print(f"DEBUG decode_base_full::: ig:{self.ig}, ERRO:{erro} modo:{self.s_gil[self.ig].modo}, chave:{chave}, tipo1:{tipo1}, tipo2:{tipo2}, cont_list_a:{cont_list_a}, cont_list_b:{cont_list_b}, cont_list_step:{cont_list_step}")
            raise ValueError(f"erro encode_base:{erro}")

        try:
            if self.s_gil[self.ig].erro != Er.er_OK:
                erro = self.s_gil[self.ig].erro  # vamos manter sempre o mesmo erro!!!
                goto_deu_erro()

            if not self.s_gil[self.ig].ativo:
                erro = Er.er_24
                goto_deu_erro()

            if self.s_gil[self.ig].tipo_operacao != Const.e_OPER_DECODE:
                erro = Er.er_OPER
                goto_deu_erro()

            if not self.CheckModeFULL():
                erro = Er.er_25
                goto_deu_erro()

            if chave > self.s_gil[self.ig].cont_itens2:
                # quer ler uma chave maior do que a contagem total
                # vamos varrer todas as chaves até achar a 'chave' desejada mas só funciona no modo 'GSON_MODO_FULL'!!!
                erro = Er.er_26
                goto_deu_erro()

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
                    self.s_gil[self.ig].pos_bytes = Const.OFFSET_MODO_FULL
                else:
                    bypass = 0  # é a que estamos, está na sequencia crescente correta
                    self.s_gil[self.ig].chave_atual = chave  # salva a última feita ok
                    self.s_gil[self.ig].cont_itens_old = self.s_gil[self.ig].cont_itens

                if self.CheckNotDinamic():
                    if self.CheckModeKV():
                        # nome da chave
                        len_chave = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
                        self.s_gil[self.ig].pos_bytes += 1
                        data = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes:self.s_gil[self.ig].pos_bytes + len_chave]
                        self.s_gil[self.ig].pos_bytes += len_chave
                        nome_chave = data.decode(encoding='utf-8')
                        # len vai ser menor que 'LEN_MAX_CHAVE_NOME' caracteres...
                    # não estamos no modo dinamico lista
                    pos_bytes = self.s_gil[self.ig].pos_bytes
                else:
                    # estamos no modo dinamico lista
                    pos_bytes = self.s_gil[self.ig].pos_bytes_dl  # onde se encontra a parte do header e está no ponto onde depende do 'item' que foi setado em 'gilson_decode_dl()'
                    # e lembrando que 's_gil[ig].pos_bytes' está ja em ponto da 'data' de fato!!! e na sequencia continua dos 'itens' chamados em ordem!!!!
                    # OBS: até entao ele ja validou o header mas aqui abaixo vai fazer tudo novamente... mas vamos melhorar isso no futuro...

                tipo_mux = self.s_gil[self.ig].bufr[pos_bytes]
                pos_bytes += 1

                if tipo_mux != Const.TIPO_GSON_NULL:
                    tipo1 = tipo_mux >> 5
                    tipo2 = tipo_mux & 0b11111

                    if tipo1 >= Tipo1.MAX:
                        erro = Er.er_27
                        goto_deu_erro()

                    if tipo2 >= Tipo2.tMAX:
                        erro = Er.er_27b
                        goto_deu_erro()

                    if tipo1 == Tipo1.LIST:
                        vezes = struct.unpack("H", self.s_gil[self.ig].bufr[pos_bytes: pos_bytes + 2])[0]
                        pos_bytes += 2

                        if vezes == 0:
                            erro = Er.er_28
                            goto_deu_erro()

                        if tipo2 == Tipo2.tSTRING:
                            cont_list_b = self.s_gil[self.ig].bufr[pos_bytes]
                            pos_bytes += 1

                            if cont_list_b == 0:
                                erro = Er.er_29
                                goto_deu_erro()
                    elif tipo1 == Tipo1.MTX2D:
                        cont_list_a = self.s_gil[self.ig].bufr[pos_bytes]
                        pos_bytes += 1
                        cont_list_b = struct.unpack("H", self.s_gil[self.ig].bufr[pos_bytes: pos_bytes + 2])[0]
                        pos_bytes += 2
                        cont_list_step = struct.unpack("H", self.s_gil[self.ig].bufr[pos_bytes: pos_bytes + 2])[0]
                        pos_bytes += 2

                        if cont_list_a == 0 or cont_list_b == 0 or cont_list_step == 0:
                            erro = Er.er_30
                            goto_deu_erro()

                        vezes = cont_list_a * cont_list_b  # no python fizemos assim pois salva exato o que temos e não é baseado em 'cont_list_step' como no C
                    else:
                        vezes = 1
                        # caso seja 'tipo2==Tipo2.tSTRING': 'cont_list_b' só precisa na codificacao, agora é com base no 'len' da vez

                    # terminada os decodes de tipos...
                    if self.CheckNotDinamic():
                        self.s_gil[self.ig].pos_bytes = pos_bytes  # segue o baile de onde estava...
                    else:
                        self.s_gil[self.ig].pos_bytes_dl = pos_bytes  # para a próxima vez...
                        # lembrando que 's_gil[ig].pos_bytes' ja está na posição correta da sequência

                    if tipo2 == Tipo2.tBIT:
                        ...  # nada ainda...
                    elif tipo2 == Tipo2.tINT8:
                        nbytes = tp.int8
                        nbytes2 = 1
                    elif tipo2 == Tipo2.tUINT8:
                        nbytes = tp.uint8
                        nbytes2 = 1
                    elif tipo2 == Tipo2.tINT16:
                        nbytes = tp.int16
                        nbytes2 = 2
                    elif tipo2 == Tipo2.tUINT16:
                        nbytes = tp.uint16
                        nbytes2 = 2
                    elif tipo2 == Tipo2.tINT32:
                        nbytes = tp.int32
                        nbytes2 = 4
                    elif tipo2 == Tipo2.tUINT32:
                        nbytes = tp.uint32
                        nbytes2 = 4
                    elif tipo2 == Tipo2.tINT64:
                        nbytes = tp.int64
                        nbytes2 = 8
                    elif tipo2 == Tipo2.tUINT64:
                        nbytes = tp.uint64
                        nbytes2 = 8
                    elif tipo2 == Tipo2.tFLOAT32:
                        nbytes = tp.float32
                        nbytes2 = 4
                    elif tipo2 == Tipo2.tFLOAT64:
                        nbytes = tp.float64
                        nbytes2 = 8
                    elif tipo2 == Tipo2.tSTRING:
                        ...  # só vai...
                    else:
                        erro = Er.er_31
                        goto_deu_erro()

                    """
                    print(f"0 decode full: modo:{self.s_gil[self.ig].modo}, chave:{chave}, nome_chave:{nome_chave}, tipo1:{tipo1}, tipo2:{tipo2}, cont_list_a:{cont_list_a}, "
                          f"cont_list_b:{cont_list_b}, cont_list_step:{cont_list_step}, nbytes:{nbytes}, nbytes2:{nbytes2}, vezes:{vezes}, bypass:{bypass}, cont:{self.s_gil[self.ig].cont_itens}, erro:{erro}")
                    """

                    if tipo2 == Tipo2.tSTRING:
                        multi_data = []
                        for i in range(vezes):
                            lens = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
                            self.s_gil[self.ig].pos_bytes += 1
                            data = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes:self.s_gil[self.ig].pos_bytes + lens]
                            self.s_gil[self.ig].pos_bytes += lens
                            if bypass == 0:
                                multi_data.append(data.decode(encoding='utf-8'))
                    elif tipo2 == Tipo2.tBIT:
                        ...  # nada ainda...
                    else:
                        # para o resto é só varer bytes
                        if tipo1 == Tipo1.MTX2D:
                            lenb = vezes * nbytes2
                            b = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes:self.s_gil[self.ig].pos_bytes + lenb]
                            self.s_gil[self.ig].pos_bytes += lenb
                            if bypass == 0:
                                """
                                data = np.frombuffer(b, dtype=nbytes)
                                data = data.reshape((cont_list_a, cont_list_b))
                                multi_data = data.tolist()
                                """
                                multi_data = bytes2data(b, nbytes, 1, cont_list_a, cont_list_b)
                        else:
                            lenb = vezes * nbytes2
                            b = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes:self.s_gil[self.ig].pos_bytes + lenb]
                            self.s_gil[self.ig].pos_bytes += lenb
                            if bypass == 0:
                                """
                                data = np.frombuffer(b, dtype=nbytes)
                                multi_data = data.tolist()
                                """
                                multi_data = bytes2data(b, nbytes)
                else:
                    if bypass == 0:
                        self.s_gil[self.ig].chaves_null += 1
                        # tem que definir o retorno de 'multi_data' e ainda saber se é lista.... 0, [], ""
                        # 255 e 255, não tem como saber que tipo era... vai ficar como None, isso é ruim!!!
                        """
                        if tipo1 == Tipo1.SINGLE:
                            if tipo2 == Tipo2.tSTRING:
                                multi_data = ""
                            else:
                                multi_data = 0
                        else:
                            multi_data = []
                        print("aaaaaaaaaaaaaaaaaa", multi_data, tipo1, tipo2)
                        """

                        # terminada os decodes de tipos...
                        if self.CheckNotDinamic():
                            self.s_gil[self.ig].pos_bytes = pos_bytes  # segue o baile de onde estava...
                        else:
                            self.s_gil[self.ig].pos_bytes_dl = pos_bytes  # para a próxima vez...
                            # lembrando que 's_gil[ig].pos_bytes' ja está na posição correta da sequência

                if erro == Er.er_OK:
                    self.s_gil[self.ig].cont_itens += 1
                    if bypass == 0:
                        if vezes == 1 and tipo1 == Tipo1.SINGLE:
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
        except Exception as err:  # except ValueError:
            #print("Erro detectado:", err)
            if erro == Er.er_OK:
                erro = Er.er_DESC7
                self.s_gil[self.ig].erro = erro

        if self.CheckModeKV():
            ret = erro, nome_chave, multi_data
        else:
            ret = erro, multi_data
        return ret

    def decode(self, chave: int, *args):
        # args vai ser uma tupla do tamanho dinamico...
        cont_list_a, cont_list_b, cont_list_step = 0, 0, 0
        tipo1 = 255
        tipo2 = 255
        modofull = 0
        i = 0

        if self.CheckModeFULL():
            modofull = 1

        # 'nome_chave' não vem por aqui... ele vai ser retornado no return junto com outros dados
        # 'valor' não vem por aqui... ele vai ser retornado no return junto com outros dados

        try:
            if modofull == 0:
                tipo1 = args[i]
                i += 1
                tipo2 = args[i]
                i += 1

                if tipo1 == Tipo1.SINGLE:
                    if tipo2 == Tipo2.tSTRING:
                        cont_list_a = args[i]
                        i += 1
                elif tipo1 == Tipo1.LIST:
                    cont_list_a = args[i]
                    i += 1
                    if tipo2 == Tipo2.tSTRING:
                        cont_list_b = args[i]
                        i += 1
                elif tipo1 == Tipo1.MTX2D:
                    cont_list_a = args[i]
                    i += 1
                    cont_list_b = args[i]
                    i += 1
                    cont_list_step = args[i]
                    i += 1
                else:
                    # erro...
                    ...

                if not isinstance(tipo1, int) or not isinstance(tipo2, int) or not isinstance(cont_list_a, int) or not isinstance(cont_list_b, int) or not isinstance(cont_list_step, int):
                    erro = Er.er_INSTANCE5
                    self.s_gil[self.ig].erro = erro
                    if self.CheckModeKV():
                        ret = erro, None, None
                    else:
                        ret = erro, None
                    return ret

                return self.decode_base(chave, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step)
        except Exception as err:  # except ValueError:
            # print("Erro detectado:", err)
            erro = Er.er_DESC14
            self.s_gil[self.ig].erro = erro
            return erro
        else:
            return self.decode_base_full(chave)

    def decode_key(self, pack: bytes | bytearray, chave: int):
        erro = Er.er_OK
        erro, modo = self.decode_init(pack)
        if erro == 0 and modo == Modo.FULL:
            erro = self.decode(chave)
            ret = self.decode_end()  # erro, total de bytes, crc e buffer cru...
            if erro == 0:
                erro = ret[0]

        if USO_DEBUG_LIB:
            print(f"DEBUG decode_key::: ig:{self.ig}, ERRO:{erro} modo:{self.s_gil[self.ig].modo}|{modo}, chave:{chave}")

        return erro

    def decode_dl_init(self, chave: int):
        # OBS: não foi testado ainda em modo KV
        erro = Er.er_OK

        def goto_deu_erro():
            self.s_gil[self.ig].erro = erro
            if erro != Er.er_OK:
                if USO_DEBUG_LIB:
                    print(f"DEBUG decode_dl_init::: ig:{self.ig}, ERRO:{erro} modo:{self.s_gil[self.ig].modo}, chave:{chave}, tam_list:{self.s_gil[self.ig].tam_list}, nitens:{self.s_gil[self.ig].nitens}")
            raise ValueError(f"erro encode_base:{erro}")

        try:
            if self.s_gil[self.ig].erro != Er.er_OK:
                erro = self.s_gil[self.ig].erro  # vamos manter sempre o mesmo erro!!!
                goto_deu_erro()

            if self.s_gil[self.ig].tipo_dinamico == 1:
                # ja está ativo esse modo... tem que teminar antes para iniciar um novo
                erro = Er.er_32b
                goto_deu_erro()

            if chave > self.s_gil[self.ig].cont_itens2:
                erro = Er.er_62
                goto_deu_erro()

            if self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes] == Const.TIPO_GSON_LDIN:

                self.s_gil[self.ig].tipo_operacao = Const.e_OPER_DECODE
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
                erro = Er.er_47
                goto_deu_erro()

            # vamos varrer nosso header para saber onde termina para entao saber onde começa os dados de fato
            for i in range(self.s_gil[self.ig].nitens):
                tipo_mux = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
                self.s_gil[self.ig].pos_bytes += 1

                # 0baaabbbbb = a:tipo1, b=tipo2
                tipo1 = tipo_mux >> 5
                tipo2 = tipo_mux & 0b11111

                if tipo1 >= Tipo1.MAX:
                    erro = Er.er_48
                    goto_deu_erro()

                if tipo2 >= Tipo2.tMAX:
                    erro = Er.er_48b
                    goto_deu_erro()

                if tipo1 == Tipo1.LIST:
                    vezes = struct.unpack("H", self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes: self.s_gil[self.ig].pos_bytes + 2])[0]
                    self.s_gil[self.ig].pos_bytes += 2

                    if vezes == 0:
                        erro = Er.er_49
                        goto_deu_erro()

                    if tipo2 == Tipo2.tSTRING:
                        cont_list_b = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
                        self.s_gil[self.ig].pos_bytes += 1

                        if cont_list_b == 0:
                            erro = Er.er_50
                            goto_deu_erro()
                elif tipo1 == Tipo1.MTX2D:
                    cont_list_a = self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes]
                    self.s_gil[self.ig].pos_bytes += 1
                    cont_list_b = struct.unpack("H", self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes: self.s_gil[self.ig].pos_bytes + 2])[0]
                    self.s_gil[self.ig].pos_bytes += 2
                    cont_list_step = struct.unpack("H", self.s_gil[self.ig].bufr[self.s_gil[self.ig].pos_bytes: self.s_gil[self.ig].pos_bytes + 2])[0]
                    self.s_gil[self.ig].pos_bytes += 2

                    if cont_list_a == 0 or cont_list_b == 0 or cont_list_step == 0:
                        erro = Er.er_51
                        goto_deu_erro()

                    vezes = cont_list_a * cont_list_b  # no python fizemos assim pois salva exato o que temos e não é baseado em 'cont_list_step' como no C
                else:
                    vezes = 1
                    # caso seja 'tipo2==Tipo2.tSTRING': 'cont_list_b' só precisa na codificacao, agora é com base no 'len' da vez

            self.s_gil[self.ig].pos_tipo_dl_end = self.s_gil[self.ig].pos_bytes  # onde começa a 'data' de fato!!!!!

            if erro == Er.er_OK:
                self.s_gil[self.ig].cont_itens += 1

        except Exception as err:  # except ValueError:
            #print("Erro detectado:", err)
            if erro == Er.er_OK:
                erro = Er.er_DESC8
                self.s_gil[self.ig].erro = erro

        return erro

    def decode_dl_data(self, item: int):
        erro = Er.er_OK
        multi_data = None

        def goto_deu_erro():
            self.s_gil[self.ig].erro = erro
            if erro != Er.er_OK:
                if USO_DEBUG_LIB:
                    print(f"DEBUG decode_dl_data::: ig:{self.ig}, ERRO:{erro} modo:{self.s_gil[self.ig].modo}, item:{item}, chave_dl:{self.s_gil[self.ig].chave_dl}")
            #return erro, multi_data
            raise ValueError(f"erro encode_base:{erro}")

        try:
            if self.CheckModeZIP():
                erro = Er.er_52
                goto_deu_erro()

            if item > self.s_gil[self.ig].nitens:
                # quer add uma item maior do que a contagem crescente... não tem como
                erro = Er.er_53
                goto_deu_erro()

            if self.s_gil[self.ig].tipo_operacao != Const.e_OPER_DECODE:
                erro = Er.er_OPER
                goto_deu_erro()

            # com base no 'item' vamos deixar o offset pronto para em 'gilson_decode_data_full_base' saber como resgatar o 'tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step' do header
            if item == 0:
                self.s_gil[self.ig].pos_bytes_dl = self.s_gil[self.ig].pos_tipo_dl_init  # posicao original do 'header'

            ret = self.decode_base_full(self.s_gil[self.ig].chave_dl)  # erro, nome_chave, multi_data
            erro = ret[0]
            multi_data = ret[-1]

            if erro == Er.er_OK:
                self.s_gil[self.ig].cont_tipo_dinamico += 1

        except Exception as err:  # except ValueError:
            #print("Erro detectado:", err)
            if erro == Er.er_OK:
                erro = Er.er_DESC9
                self.s_gil[self.ig].erro = erro

        return erro, multi_data

    def decode_dl_end(self):
        erro = Er.er_OK

        if self.s_gil[self.ig].tipo_dinamico == 0:
            erro = Er.er_54
        elif self.s_gil[self.ig].tipo_operacao != Const.e_OPER_DECODE:
            erro = Er.er_55
        elif self.s_gil[self.ig].cont_tipo_dinamico != (self.s_gil[self.ig].tam_list * self.s_gil[self.ig].nitens):
            erro = Er.er_56

        if erro != Er.er_OK:
            if USO_DEBUG_LIB:
                print(f"DEBUG decode_dl_end::: ig:{self.ig}, ERRO:{erro} modo:{self.s_gil[self.ig].modo}, tam_list:{self.s_gil[self.ig].tam_list}, nitens:{self.s_gil[self.ig].nitens}, tipo_dinamico:{self.s_gil[self.ig].cont_tipo_dinamico}")

        self.s_gil[self.ig].tipo_dinamico = 0  # finalizando o tratar um tipo dinamico

        return erro

    def decode_mapfix(self, mapa: list | tuple):
        cont_list_a, cont_list_b, cont_list_step = 0, 0, 0
        modofull = 0

        if self.CheckModeFULL():
            modofull = 1

        chave = mapa[0]
        tipo1 = mapa[1]
        tipo2 = mapa[2]

        if modofull == 0:
            if tipo1 == Tipo1.SINGLE:
                if tipo2 == Tipo2.tSTRING:
                    cont_list_a = mapa[3]
            elif tipo1 == Tipo1.LIST:
                cont_list_a = mapa[3]
                if tipo2 == Tipo2.tSTRING:
                    cont_list_b = mapa[4]
            elif tipo1 == Tipo1.MTX2D:
                cont_list_a = mapa[3]
                cont_list_b = mapa[4]
                cont_list_step = mapa[5]
            else:
                # erro...
                ...

            return self.decode_base(chave, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step)
        else:
            return self.decode_base_full(chave)

    def decode_mapdin(self, mapa: list | tuple, *args):
        cont_list_a, cont_list_b, cont_list_step = 0, 0, 0
        modofull = 0
        i = 0

        if self.CheckModeFULL():
            modofull = 1

        chave = mapa[0]
        tipo1 = mapa[1]
        tipo2 = mapa[2]

        if modofull == 0:
            if tipo1 == Tipo1.SINGLE:
                if tipo2 == Tipo2.tSTRING:
                    cont_list_a = args[i]
                    i += 1
            elif tipo1 == Tipo1.LIST:
                cont_list_a = args[i]
                i += 1
                if tipo2 == Tipo2.tSTRING:
                    cont_list_b = args[i]
                    i += 1
            elif tipo1 == Tipo1.MTX2D:
                cont_list_a = args[i]
                i += 1
                cont_list_b = args[i]
                i += 1
                cont_list_step = args[i]
                i += 1
            else:
                # erro...
                ...

            if not isinstance(tipo1, int) or not isinstance(tipo2, int) or not isinstance(cont_list_a, int) or not isinstance(cont_list_b, int) or not isinstance(cont_list_step, int):
                erro = Er.er_INSTANCE6
                self.s_gil[self.ig].erro = erro
                if self.CheckModeKV():
                    ret = erro, None, None
                else:
                    ret = erro, None
                return ret

            return self.decode_base(chave, tipo1, tipo2, cont_list_a, cont_list_b, cont_list_step)
        else:
            return self.decode_base_full(chave)

    def decode_u8(self, chave: int):
        return self.decode_base(chave, Tipo1.SINGLE, Tipo2.tUINT8, 0, 0, 0)

    def decode_s8(self, chave: int):
        return self.decode_base(chave, Tipo1.SINGLE, Tipo2.tINT8, 0, 0, 0)

    def decode_u16(self, chave: int):
        return self.decode_base(chave, Tipo1.SINGLE, Tipo2.tUINT16, 0, 0, 0)

    def decode_s16(self, chave: int):
        return self.decode_base(chave, Tipo1.SINGLE, Tipo2.tINT16, 0, 0, 0)

    def decode_u32(self, chave: int):
        return self.decode_base(chave, Tipo1.SINGLE, Tipo2.tUINT32, 0, 0, 0)

    def decode_s32(self, chave: int):
        return self.decode_base(chave, Tipo1.SINGLE, Tipo2.tINT32, 0, 0, 0)

    def decode_u64(self, chave: int):
        return self.decode_base(chave, Tipo1.SINGLE, Tipo2.tUINT64, 0, 0, 0)

    def decode_s64(self, chave: int):
        return self.decode_base(chave, Tipo1.SINGLE, Tipo2.tINT64, 0, 0, 0)

    def decode_f32(self, chave: int):
        return self.decode_base(chave, Tipo1.SINGLE, Tipo2.tFLOAT32, 0, 0, 0)

    def decode_f64(self, chave: int):
        return self.decode_base(chave, Tipo1.SINGLE, Tipo2.tFLOAT64, 0, 0, 0)

    def decode_str(self, chave: int, cont_list_a: int):
        return self.decode_base(chave, Tipo1.SINGLE, Tipo2.tSTRING, cont_list_a, 0, 0)

    def decode_lu8(self, chave: int, cont_list_a: int):
        return self.decode_base(chave, Tipo1.LIST, Tipo2.tUINT8, cont_list_a, 0, 0)

    def decode_ls8(self, chave: int, cont_list_a: int):
        return self.decode_base(chave, Tipo1.LIST, Tipo2.tINT8, cont_list_a, 0, 0)

    def decode_lu16(self, chave: int, cont_list_a: int):
        return self.decode_base(chave, Tipo1.LIST, Tipo2.tUINT16, cont_list_a, 0, 0)

    def decode_ls16(self, chave: int, cont_list_a: int):
        return self.decode_base(chave, Tipo1.LIST, Tipo2.tINT16, cont_list_a, 0, 0)

    def decode_lu32(self, chave: int, cont_list_a: int):
        return self.decode_base(chave, Tipo1.LIST, Tipo2.tUINT32, cont_list_a, 0, 0)

    def decode_ls32(self, chave: int, cont_list_a: int):
        return self.decode_base(chave, Tipo1.LIST, Tipo2.tINT32, cont_list_a, 0, 0)

    def decode_lu64(self, chave: int, cont_list_a: int):
        return self.decode_base(chave, Tipo1.LIST, Tipo2.tUINT64, cont_list_a, 0, 0)

    def decode_ls64(self, chave: int, cont_list_a: int):
        return self.decode_base(chave, Tipo1.LIST, Tipo2.tINT64, cont_list_a, 0, 0)

    def decode_lf32(self, chave: int, cont_list_a: int):
        return self.decode_base(chave, Tipo1.LIST, Tipo2.tFLOAT32, cont_list_a, 0, 0)

    def decode_lf64(self, chave: int, cont_list_a: int):
        return self.decode_base(chave, Tipo1.LIST, Tipo2.tFLOAT64, cont_list_a, 0, 0)

    def decode_lstr(self, chave: int, cont_list_a: int, cont_list_b: int):
        return self.decode_base(chave, Tipo1.LIST, Tipo2.tSTRING, cont_list_a, cont_list_b, 0)

    def decode_mu8(self, chave: int, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.decode_base(chave, Tipo1.MTX2D, Tipo2.tUINT8, cont_list_a, cont_list_b, cont_list_step)

    def decode_ms8(self, chave: int, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.decode_base(chave, Tipo1.MTX2D, Tipo2.tINT8, cont_list_a, cont_list_b, cont_list_step)

    def decode_mu16(self, chave: int, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.decode_base(chave, Tipo1.MTX2D, Tipo2.tUINT16, cont_list_a, cont_list_b, cont_list_step)

    def decode_ms16(self, chave: int, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.decode_base(chave, Tipo1.MTX2D, Tipo2.tINT16, cont_list_a, cont_list_b, cont_list_step)

    def decode_mu32(self, chave: int, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.decode_base(chave, Tipo1.MTX2D, Tipo2.tUINT32, cont_list_a, cont_list_b, cont_list_step)

    def decode_ms32(self, chave: int, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.decode_base(chave, Tipo1.MTX2D, Tipo2.tINT32, cont_list_a, cont_list_b, cont_list_step)

    def decode_mu64(self, chave: int, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.decode_base(chave, Tipo1.MTX2D, Tipo2.tUINT64, cont_list_a, cont_list_b, cont_list_step)

    def decode_ms64(self, chave: int, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.decode_base(chave, Tipo1.MTX2D, Tipo2.tINT64, cont_list_a, cont_list_b, cont_list_step)

    def decode_mf32(self, chave: int, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.decode_base(chave, Tipo1.MTX2D, Tipo2.tFLOAT32, cont_list_a, cont_list_b, cont_list_step)

    def decode_mf64(self, chave: int, cont_list_a: int, cont_list_b: int, cont_list_step: int):
        return self.decode_base(chave, Tipo1.MTX2D, Tipo2.tFLOAT64, cont_list_a, cont_list_b, cont_list_step)


