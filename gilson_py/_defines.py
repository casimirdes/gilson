from enum import IntEnum

# constantes globais:
"""
GIL_MODO_ZIP = 0  # é cru, sem nada, somente a data bruta, tem 1 byte adicional que é [0]=modo
GIL_MODO_FULL = 1  # modo padão com offset, crc, identificador
GIL_MODO_KV = 2  # modo GIL_MODO_FULL + KV ala key:value chave:valor salva nome em string das chaves ala JSON da vida
GIL_MODO_KV_ZIP = 3  # modo GIL_MODO_ZIP porem salva os nomes das chaves ala JSON da vida
GIL_MODO_JSON = 4  # caso particular de encode/decode de um JSON estático *** só usa com funcoes "xxxxx_json" experimental...
GIL_MODO_MAX = 5  # indica máximo, para ser válido tem que ser menor que isso


GIL_SINGLE = 0  # valor unico
GIL_LIST = 1  # é no formato lista, [u16] mas até 64k
GIL_MTX2D = 2  # é no formato matriz, max 2 dimenções!!! [u8][u16] mas jamais pode passar de 64k!!!!
GIL_MAX = 3  # indica máximo, para ser válido tem que ser menor que isso

# reserva o '0' para outros usos...
GIL_tBIT = 1
GIL_tINT8 = 2
GIL_tUINT8 = 3
GIL_tINT16: Final[int] = 4  #GIL_tINT16 = 4
GIL_tUINT16 = 5
GIL_tINT32 = 6
GIL_tUINT32 = 7
GIL_tINT64 = 8
GIL_tUINT64 = 9
GIL_tFLOAT32 = 10
GIL_tFLOAT64 = 11
GIL_tSTRING = 12
GIL_tMAX = 13  # indica máximo, para ser válido tem que ser menor que isso

OFFSET_MODO_ZIP = 2
OFFSET_MODO_FULL = 8

PACK_MAX_BYTES = 65536  # max 65536 bytes um pacote gilson

LEN_PACKS_GILSON = 2  # 2 pacotes manipulaveis "ao mesmo tempo", se abriu o segundo tem que fechar para voltar para o primeiro!!!

TIPO_GIL_LDIN = 0b11100000  # lista dinâmica de dados de diversos tipos, limitado até 255 tipos e não pode ter lista de lista
TIPO_GIL_NULL = 0b11111111  # quando for entrar com uma data nula, vai chamar uma função específica para sinalizar que vai gravar a chave porem não terá dados

e_OPER_NULL = 0  # nenhuma ainda...
e_OPER_ENCODE = 1  # operação de encode()
e_OPER_DECODE = 2  # operação de decode()

GIL_LIMIT_KEY_NAME = 16  # tamanho máximo do nome chave de cada elemento ala JSON quando utilizado modo 'GIL_MODO_KV'
GIL_LIMIT_STRING = 255  # tamanho maximo de um item que é do tipo string, seja 'GIL_SINGLE' ou 'GIL_LIST'
GIL_LIMIT_KEYS = 255  # até 255 chaves cada pacote gilson, (0 a 254)
"""


class Const(IntEnum):
    OFFSET_MODO_ZIP = 2
    OFFSET_MODO_FULL = 8

    PACK_MAX_BYTES = 65536  # max 65536 bytes um pacote gilson

    LEN_PACKS_GILSON = 2  # 2 pacotes manipulaveis "ao mesmo tempo", se abriu o segundo tem que fechar para voltar para o primeiro!!!

    TIPO_GIL_LDIN = 0b11100000  # lista dinâmica de dados de diversos tipos, limitado até 255 tipos e não pode ter lista de lista
    TIPO_GIL_NULL = 0b11111111  # quando for entrar com uma data nula, vai chamar uma função específica para sinalizar que vai gravar a chave porem não terá dados

    e_OPER_NULL = 0  # nenhuma ainda...
    e_OPER_ENCODE = 1  # operação de encode()
    e_OPER_DECODE = 2  # operação de decode()

    GIL_LIMIT_KEYS = 255  # até 255 chaves cada pacote gilson, (0 a 254)
    GIL_LIMIT_STRING = 255  # tamanho maximo de um item que é do tipo string, seja 'GIL_SINGLE' ou 'GIL_LIST'
    GIL_LIMIT_KEY_NAME = 16  # tamanho máximo do nome chave de cada elemento ala JSON quando utilizado modo 'GIL_MODO_KV'


class Modo(IntEnum):
    """
    GIL_MODO_ZIP = 0  # é cru, sem nada, somente a data bruta, tem 1 byte adicional que é [0]=modo
    GIL_MODO_FULL = 1  # modo padão com offset, crc, identificador
    GIL_MODO_KV = 2  # modo GIL_MODO_FULL + KV ala key:value chave:valor salva nome em string das chaves ala JSON da vida
    GIL_MODO_KV_ZIP = 3  # modo GIL_MODO_ZIP porem salva os nomes das chaves ala JSON da vida
    GIL_MODO_JSON = 4  # caso particular de encode/decode de um JSON estático *** só usa com funcoes "xxxxx_json" experimental...
    GIL_MODO_MAX = 5  # indica máximo, para ser válido tem que ser menor que isso
    """
    ZIP = 0
    FULL = 1
    KV = 2
    KV_ZIP = 3
    JSON = 4
    MAX = 5

class Tipo1(IntEnum):
    """
    GIL_SINGLE = 0  # valor unico
    GIL_LIST = 1  # é no formato lista, [u16] mas até 64k
    GIL_MTX2D = 2  # é no formato matriz, max 2 dimenções!!! [u8][u16] mas jamais pode passar de 64k!!!!
    GIL_MAX = 3  # indica máximo, para ser válido tem que ser menor que isso
    """
    SINGLE = 0
    LIST = 1
    MTX2D = 2
    MAX = 3


class Tipo2(IntEnum):
    """
    # reserva o '0' para outros usos...
    GIL_tBIT = 1
    GIL_tINT8 = 2
    GIL_tUINT8 = 3
    GIL_tINT16 = 4
    GIL_tUINT16 = 5
    GIL_tINT32 = 6
    GIL_tUINT32 = 7
    GIL_tINT64 = 8
    GIL_tUINT64 = 9
    GIL_tFLOAT32 = 10
    GIL_tFLOAT64 = 11
    GIL_tSTRING = 12
    GIL_tMAX = 13  # indica máximo, para ser válido tem que ser menor que isso
    """
    tBIT = 1
    tINT8 = 2
    tUINT8 = 3
    tINT16 = 4
    tUINT16 = 5
    tINT32 = 6
    tUINT32 = 7
    tINT64 = 8
    tUINT64 = 9
    tFLOAT32 = 10
    tFLOAT64 = 11
    tSTRING = 12
    tMAX = 13


class Er(IntEnum):
    er_OK = 0
    er_STRMAX = -37  # erro tamanho de string no encode_base
    er_OPER = -59  # erro de operação, está em enconde e quer usar funcao de decode ou contrário
    er_OVER = -67  # erro de OverflowError no encode_base()
    er_DESC1 = -69  # erro desconhecido/não tratado
    er_DESC2 = -76  # erro desconhecido/não tratado
    er_DESC3 = -77  # erro desconhecido/não tratado
    er_DESC4 = -78  # erro desconhecido/não tratado
    er_DESC5 = -79  # erro desconhecido/não tratado
    er_DESC6 = -80  # erro desconhecido/não tratado
    er_DESC7 = -81  # erro desconhecido/não tratado
    er_DESC8 = -82  # erro desconhecido/não tratado
    er_DESC9 = -83  # erro desconhecido/não tratado
    er_DESC10 = -84  # erro desconhecido/não tratado
    er_DESC11 = -85  # erro desconhecido/não tratado
    er_DESC12 = -86  # erro desconhecido/não tratado
    er_DESC13 = -87  # erro desconhecido/não tratado
    er_DESC14 = -88  # erro desconhecido/não tratado
    er_INSTANCE1 = -70  # erro parametro *vars no encode()
    er_INSTANCE2 = -71  # erro parametro *vars no encode_mapfix()
    er_INSTANCE3 = -72  # erro parametro *vars no encode_mapfix()
    er_INSTANCE4 = -73  # erro parametro *vars no encode_mapdin()
    er_INSTANCE5 = -74  # erro parametro *vars no decode()
    er_INSTANCE6 = -75  # erro parametro *vars no decode_mapdin()
    er_0 = -1  # encode_init() quer iniciar e ja tem pacote ativo na classe s_gil
    er_1 = -2  # encode_init() segunda classe s_gil tambem já está ativa
    er_2 = -3  # encode_base() que usar e não tem pacote ativo, iniciado em encode_init()
    er_3 = -4  # encode_base() erro de Tipo1 inválido
    er_4 = -5  # encode_base() erro limite maior que GIL_LIMIT_KEYS
    er_5 = -6  # encode_base() modo KV e nome da chave maior que GIL_LIMIT_KEY_NAME
    er_6 = -7  # encode_base() uma lista e cont_list_a==0
    er_7 = -8  # encode_base() uma lista de string e cont_list_b==0
    er_8 = -9  # encode_base() uma matriz e cont_list_X errados
    er_9 = -10  # encode_base() string simples com cont_list_a==0
    er_10 = -11  # encode_base() erro de Tipo2 inválido
    er_11 = -12  # encode_base() tamanho da string maior que 'len_string_max' definido por 'cont_list_a'
    er_12 = -13  # decode_init() quer iniciar e ja tem pacote ativo na classe s_gil
    er_13 = -14  # decode_init() segunda classe s_gil tambem já está ativa
    er_14 = -15  # decode_init() erro calculo do CRC no modo FULL
    er_15 = -16  # decode_init() erro de Modo inválido
    er_16 = -17  # decode_end_base() contagem de bytes do pacote é diferente, no modo FULL
    er_17 = -18  # decode_base() pacote não está ativo, não chamou decode_init()
    er_18 = -19  # decode_base() erro Tipo1
    er_18b = -19  # decode_base() erro Tipo2 passo1
    er_19 = -20  # decode_base() quer add uma chave maior do que a contagem crescente...
    er_20 = -21  # decode_base() uma lista com cont_list_a==0
    er_21 = -22  # decode_base() uma lista de string com cont_list_b==0
    er_22 = -23  # decode_base() uma matriz com cont_list_X zerados...
    er_23 = -24  # decode_base() erro de Tipo2 passo2
    er_24 = -25  # decode_base_full() pacote não está ativo
    er_25 = -26  # decode_base_full() pacote não é modo FULL
    er_26 = -27  # decode_base_full() quer ler uma chave maior do que a contagem total
    er_27 = -28  # decode_base_full() erro Tipo1 inválido
    er_27b = -28  # decode_base_full() erro Tipo2 inválido
    er_28 = -29  # decode_base_full() total de elementos de uma lista zerado
    er_29 = -30  # decode_base_full() lista de string e cont_list_b==0, tamanho max de cada string
    er_30 = -31  # decode_base_full() matriz com cont_list_X zerados
    er_31 = -32  # decode_base_full() Tipo2 inválido
    er_32 = -33  # encode_base() matriz de strings não pode
    er_32b = -33  # decode_dl_init() matriz de strings não pode
    er_33 = -34  # encode_dl_init() já tem pacote ativo
    er_34 = -35  # encode_dl_init() já está ativo esse modo... tem que terminar antes para iniciar um novo
    er_35 = -36  # encode_dl_init() quer add uma chave maior do que a contagem crescente... não tem como
    er_36 = -38  # encode_base() esquema de nulo não aceita em modo ZIP!!
    er_37 = -39  # valid_encode_dl() quer validar pacote dinâmico mas não é
    er_38 = -40  # valid_encode_dl() erro Tipo1 inválido
    er_38b = -40  # valid_encode_dl() erro Tipo2 inválido
    er_39 = -41  # valid_encode_dl() quer add uma item maior do que a contagem crescente... não tem como
    er_40 = -42  # valid_encode_dl() um lista com cont_list_a==0
    er_41 = -43  # valid_encode_dl() uma lista de string com cont_list_b==0
    er_42 = -44  # valid_encode_dl() uma matriz de string não pode
    er_43 = -45  # valid_encode_dl() uma matriz com algum cont_list_X zerado
    er_44 = -46  # valid_encode_dl() string simples com cont_list_a==0
    er_45 = -47  # encode_dl_end() que finalizar pacote dinâmico mas não é dinâmico
    er_46 = -48  # encode_dl_end() total de itens gerais diferente do programado
    er_47 = -49  # decode_dl_init() que decodificar pacote dinâmico mas não é
    er_48 = -50  # decode_dl_init() erro Tipo1 inválido
    er_48b = -50  # decode_dl_init() erro Tipo2 inválido
    er_49 = -51  # decode_dl_init() tamanho da lista zerado
    er_50 = -52  # decode_dl_init() lista de string e cont_list_b==0
    er_51 = -53  # decode_dl_init() matriz com algum cont_list_X zerado
    er_52 = -54  # decode_dl_data() não aceita modo ZIP no pacote dinâmico
    er_53 = -55  # decode_dl_data() quer add uma item maior do que a contagem crescente... não tem como
    er_54 = -56  # decode_dl_end() quer finalizar um pacote dinâmico e não é
    er_55 = -57  # decode_dl_end() quer finalizar decode e o pacote é encode
    er_56 = -58  # decode_dl_end() total de itens gerais diferente do programado
    er_58 = -60  # encode_base() quer add uma chave maior do que a contagem crescente... não tem como
    er_59 = -61  # encode_base() quer add uma chave maior que a contagem parcial
    er_60 = -62  # decode_base() quer ler uma chave maior do que a total programado
    er_61 = -63  # decode_base() chave ja foi adicionada no decode
    er_62 = -64  # decode_dl_init() quer ler uma chave maior do que a total programado
    er_63 = -68  # encode_dl_init() tipo do pacote não é FULL
    er_64 = -89  # decode_key() erro ou tipo do pacote não é FULL