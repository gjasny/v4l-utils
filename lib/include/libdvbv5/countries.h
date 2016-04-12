/*
 * Copyright (C) 2006, 2007, 2008, 2009 Winfried Koehler
 * Copyright (C) 2014 Akihiro Tsukada
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation version 2.1 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. if not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * @file countries.h
 * @ingroup ancillary
 * @brief Provides ancillary code to convert ISO 3166-1 country codes
 * @copyright GNU Lesser General Public License version 2.1 (LGPLv2.1)
 * @author Winfried Koehler
 * @author Akihiro Tsukada
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

#ifndef _COUNTRIES_H_
#define _COUNTRIES_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum dvb_country_t
 * @brief ISO-3166-1 alpha-2 country code
 * @ingroup ancillary
 *
 * @var COUNTRY_UNKNOWN
 *	@brief (Unknown Country)
 * @var AD
 *	@brief Andorra
 * @var AE
 *	@brief United Arab Emirates
 * @var AF
 *	@brief Afghanistan
 * @var AG
 *	@brief Antigua and Barbuda
 * @var AI
 *	@brief Anguilla
 * @var AL
 *	@brief Albania
 * @var AM
 *	@brief Armenia
 * @var AO
 *	@brief Angola
 * @var AQ
 *	@brief Antarctica
 * @var AR
 *	@brief Argentina
 * @var AS
 *	@brief American Samoa
 * @var AT
 *	@brief Austria
 * @var AU
 *	@brief Australia
 * @var AW
 *	@brief Aruba
 * @var AX
 *	@brief Aland Islands
 * @var AZ
 *	@brief Azerbaijan
 * @var BA
 *	@brief Bosnia and Herzegovina
 * @var BB
 *	@brief Barbados
 * @var BD
 *	@brief Bangladesh
 * @var BE
 *	@brief Belgium
 * @var BF
 *	@brief Burkina Faso
 * @var BG
 *	@brief Bulgaria
 * @var BH
 *	@brief Bahrain
 * @var BI
 *	@brief Burundi
 * @var BJ
 *	@brief Benin
 * @var BL
 *	@brief Saint Barthelemy
 * @var BM
 *	@brief Bermuda
 * @var BN
 *	@brief Brunei Darussalam
 * @var BO
 *	@brief Plurinational State of Bolivia
 * @var BQ
 *	@brief Bonaire, Saint Eustatius and Saba
 * @var BR
 *	@brief Brazil
 * @var BS
 *	@brief Bahamas
 * @var BT
 *	@brief Bhutan
 * @var BV
 *	@brief Bouvet Island
 * @var BW
 *	@brief Botswana
 * @var BY
 *	@brief Belarus
 * @var BZ
 *	@brief Belize
 * @var CA
 *	@brief Canada
 * @var CC
 *	@brief Cocos (Keeling) Islands
 * @var CD
 *	@brief The Democratic Republic of the Congo
 * @var CF
 *	@brief Central African Republic
 * @var CG
 *	@brief Congo
 * @var CH
 *	@brief Switzerland
 * @var CI
 *	@brief Cote d'Ivoire
 * @var CK
 *	@brief Cook Islands
 * @var CL
 *	@brief Chile
 * @var CM
 *	@brief Cameroon
 * @var CN
 *	@brief China
 * @var CO
 *	@brief Colombia
 * @var CR
 *	@brief Costa Rica
 * @var CU
 *	@brief Cuba
 * @var CV
 *	@brief Cape Verde
 * @var CW
 *	@brief Curacao
 * @var CX
 *	@brief Christmas Island
 * @var CY
 *	@brief Cyprus
 * @var CZ
 *	@brief Czech Republic
 * @var DE
 *	@brief Germany
 * @var DJ
 *	@brief Djibouti
 * @var DK
 *	@brief Denmark
 * @var DM
 *	@brief Dominica
 * @var DO
 *	@brief Dominican Republic
 * @var DZ
 *	@brief Algeria
 * @var EC
 *	@brief Ecuador
 * @var EE
 *	@brief Estonia
 * @var EG
 *	@brief Egypt
 * @var EH
 *	@brief Western Sahara
 * @var ER
 *	@brief Eritrea
 * @var ES
 *	@brief Spain
 * @var ET
 *	@brief Ethiopia
 * @var FI
 *	@brief Finland
 * @var FJ
 *	@brief Fiji
 * @var FK
 *	@brief Falkland Islands (Malvinas)
 * @var FM
 *	@brief Federated States of Micronesia
 * @var FO
 *	@brief Faroe Islands
 * @var FR
 *	@brief France
 * @var GA
 *	@brief Gabon
 * @var GB
 *	@brief United Kingdom
 * @var GD
 *	@brief Grenada
 * @var GE
 *	@brief Georgia
 * @var GF
 *	@brief French Guiana
 * @var GG
 *	@brief Guernsey
 * @var GH
 *	@brief Ghana
 * @var GI
 *	@brief Gibraltar
 * @var GL
 *	@brief Greenland
 * @var GM
 *	@brief Gambia
 * @var GN
 *	@brief Guinea
 * @var GP
 *	@brief Guadeloupe
 * @var GQ
 *	@brief Equatorial Guinea
 * @var GR
 *	@brief Greece
 * @var GS
 *	@brief South Georgia and the South Sandwich Islands
 * @var GT
 *	@brief Guatemala
 * @var GU
 *	@brief Guam
 * @var GW
 *	@brief Guinea-Bissau
 * @var GY
 *	@brief Guyana
 * @var HK
 *	@brief Hong Kong
 * @var HM
 *	@brief Heard Island and McDonald Islands
 * @var HN
 *	@brief Honduras
 * @var HR
 *	@brief Croatia
 * @var HT
 *	@brief Haiti
 * @var HU
 *	@brief Hungary
 * @var ID
 *	@brief Indonesia
 * @var IE
 *	@brief Ireland
 * @var IL
 *	@brief Israel
 * @var IM
 *	@brief Isle of Man
 * @var IN
 *	@brief India
 * @var IO
 *	@brief British Indian Ocean Territory
 * @var IQ
 *	@brief Iraq
 * @var IR
 *	@brief Islamic Republic of Iran
 * @var IS
 *	@brief Iceland
 * @var IT
 *	@brief Italy
 * @var JE
 *	@brief Jersey
 * @var JM
 *	@brief Jamaica
 * @var JO
 *	@brief Jordan
 * @var JP
 *	@brief Japan
 * @var KE
 *	@brief Kenya
 * @var KG
 *	@brief Kyrgyzstan
 * @var KH
 *	@brief Cambodia
 * @var KI
 *	@brief Kiribati
 * @var KM
 *	@brief Comoros
 * @var KN
 *	@brief Saint Kitts and Nevis
 * @var KP
 *	@brief Democratic People's Republic of Korea
 * @var KR
 *	@brief Republic of Korea
 * @var KW
 *	@brief Kuwait
 * @var KY
 *	@brief Cayman Islands
 * @var KZ
 *	@brief Kazakhstan
 * @var LA
 *	@brief Lao People's Democratic Republic
 * @var LB
 *	@brief Lebanon
 * @var LC
 *	@brief Saint Lucia
 * @var LI
 *	@brief Liechtenstein
 * @var LK
 *	@brief Sri Lanka
 * @var LR
 *	@brief Liberia
 * @var LS
 *	@brief Lesotho
 * @var LT
 *	@brief Lithuania
 * @var LU
 *	@brief Luxembourg
 * @var LV
 *	@brief Latvia
 * @var LY
 *	@brief Libyan Arab Jamahiriya
 * @var MA
 *	@brief Morocco
 * @var MC
 *	@brief Monaco
 * @var MD
 *	@brief Republic of Moldova
 * @var ME
 *	@brief Montenegro
 * @var MF
 *	@brief Saint Martin (French part)
 * @var MG
 *	@brief Madagascar
 * @var MH
 *	@brief Marshall Islands
 * @var MK
 *	@brief The Former Yugoslav Republic of Macedonia
 * @var ML
 *	@brief Mali
 * @var MM
 *	@brief Myanmar
 * @var MN
 *	@brief Mongolia
 * @var MO
 *	@brief Macao
 * @var MP
 *	@brief Northern Mariana Islands
 * @var MQ
 *	@brief Martinique
 * @var MR
 *	@brief Mauritania
 * @var MS
 *	@brief Montserrat
 * @var MT
 *	@brief Malta
 * @var MU
 *	@brief Mauritius
 * @var MV
 *	@brief Maldives
 * @var MW
 *	@brief Malawi
 * @var MX
 *	@brief Mexico
 * @var MY
 *	@brief Malaysia
 * @var MZ
 *	@brief Mozambique
 * @var NA
 *	@brief Namibia
 * @var NC
 *	@brief New Caledonia
 * @var NE
 *	@brief Niger
 * @var NF
 *	@brief Norfolk Island
 * @var NG
 *	@brief Nigeria
 * @var NI
 *	@brief Nicaragua
 * @var NL
 *	@brief Netherlands
 * @var NO
 *	@brief Norway
 * @var NP
 *	@brief Nepal
 * @var NR
 *	@brief Nauru
 * @var NU
 *	@brief Niue
 * @var NZ
 *	@brief New Zealand
 * @var OM
 *	@brief Oman
 * @var PA
 *	@brief Panama
 * @var PE
 *	@brief Peru
 * @var PF
 *	@brief French Polynesia
 * @var PG
 *	@brief Papua New Guinea
 * @var PH
 *	@brief Philippines
 * @var PK
 *	@brief Pakistan
 * @var PL
 *	@brief Poland
 * @var PM
 *	@brief Saint Pierre and Miquelon
 * @var PN
 *	@brief Pitcairn
 * @var PR
 *	@brief Puerto Rico
 * @var PS
 *	@brief Occupied Palestinian Territory
 * @var PT
 *	@brief Portugal
 * @var PW
 *	@brief Palau
 * @var PY
 *	@brief Paraguay
 * @var QA
 *	@brief Qatar
 * @var RE
 *	@brief Reunion
 * @var RO
 *	@brief Romania
 * @var RS
 *	@brief Serbia
 * @var RU
 *	@brief Russian Federation
 * @var RW
 *	@brief Rwanda
 * @var SA
 *	@brief Saudi Arabia
 * @var SB
 *	@brief Solomon Islands
 * @var SC
 *	@brief Seychelles
 * @var SD
 *	@brief Sudan
 * @var SE
 *	@brief Sweden
 * @var SG
 *	@brief Singapore
 * @var SH
 *	@brief Saint Helena, Ascension and Tristan da Cunha
 * @var SI
 *	@brief Slovenia
 * @var SJ
 *	@brief Svalbard and Jan Mayen
 * @var SK
 *	@brief Slovakia
 * @var SL
 *	@brief Sierra Leone
 * @var SM
 *	@brief San Marino
 * @var SN
 *	@brief Senegal
 * @var SO
 *	@brief Somalia
 * @var SR
 *	@brief Suriname
 * @var SS
 *	@brief South Sudan
 * @var ST
 *	@brief Sao Tome and Principe
 * @var SV
 *	@brief El Salvador
 * @var SX
 *	@brief Sint Maarten (Dutch part)
 * @var SY
 *	@brief Syrian Arab Republic
 * @var SZ
 *	@brief Swaziland
 * @var TC
 *	@brief Turks and Caicos Islands
 * @var TD
 *	@brief Chad
 * @var TF
 *	@brief French Southern Territories
 * @var TG
 *	@brief Togo
 * @var TH
 *	@brief Thailand
 * @var TJ
 *	@brief Tajikistan
 * @var TK
 *	@brief Tokelau
 * @var TL
 *	@brief Timor-Leste
 * @var TM
 *	@brief Turkmenistan
 * @var TN
 *	@brief Tunisia
 * @var TO
 *	@brief Tonga
 * @var TR
 *	@brief Turkey
 * @var TT
 *	@brief Trinidad and Tobago
 * @var TV
 *	@brief Tuvalu
 * @var TW
 *	@brief Taiwan, Province of China
 * @var TZ
 *	@brief United Republic of Tanzania
 * @var UA
 *	@brief Ukraine
 * @var UG
 *	@brief Uganda
 * @var UM
 *	@brief United States Minor Outlying Islands
 * @var US
 *	@brief United States
 * @var UY
 *	@brief Uruguay
 * @var UZ
 *	@brief Uzbekistan
 * @var VA
 *	@brief Holy See (Vatican City State)
 * @var VC
 *	@brief Saint Vincent and The Grenadines
 * @var VE
 *	@brief Bolivarian Republic of Venezuela
 * @var VG
 *	@brief British Virgin Islands
 * @var VI
 *	@brief U.S. Virgin Islands
 * @var VN
 *	@brief Viet Nam
 * @var VU
 *	@brief Vanuatu
 * @var WF
 *	@brief Wallis and Futuna
 * @var WS
 *	@brief Samoa
 * @var YE
 *	@brief Yemen
 * @var YT
 *	@brief Mayotte
 * @var ZA
 *	@brief South Africa
 * @var ZM
 *	@brief Zambia
 * @var ZW
 *	@brief Zimbabwe
 */
enum dvb_country_t {
    COUNTRY_UNKNOWN,

    AD,
    AE,
    AF,
    AG,
    AI,
    AL,
    AM,
    AO,
    AQ,
    AR,
    AS,
    AT,
    AU,
    AW,
    AX,
    AZ,
    BA,
    BB,
    BD,
    BE,
    BF,
    BG,
    BH,
    BI,
    BJ,
    BL,
    BM,
    BN,
    BO,
    BQ,
    BR,
    BS,
    BT,
    BV,
    BW,
    BY,
    BZ,
    CA,
    CC,
    CD,
    CF,
    CG,
    CH,
    CI,
    CK,
    CL,
    CM,
    CN,
    CO,
    CR,
    CU,
    CV,
    CW,
    CX,
    CY,
    CZ,
    DE,
    DJ,
    DK,
    DM,
    DO,
    DZ,
    EC,
    EE,
    EG,
    EH,
    ER,
    ES,
    ET,
    FI,
    FJ,
    FK,
    FM,
    FO,
    FR,
    GA,
    GB,
    GD,
    GE,
    GF,
    GG,
    GH,
    GI,
    GL,
    GM,
    GN,
    GP,
    GQ,
    GR,
    GS,
    GT,
    GU,
    GW,
    GY,
    HK,
    HM,
    HN,
    HR,
    HT,
    HU,
    ID,
    IE,
    IL,
    IM,
    IN,
    IO,
    IQ,
    IR,
    IS,
    IT,
    JE,
    JM,
    JO,
    JP,
    KE,
    KG,
    KH,
    KI,
    KM,
    KN,
    KP,
    KR,
    KW,
    KY,
    KZ,
    LA,
    LB,
    LC,
    LI,
    LK,
    LR,
    LS,
    LT,
    LU,
    LV,
    LY,
    MA,
    MC,
    MD,
    ME,
    MF,
    MG,
    MH,
    MK,
    ML,
    MM,
    MN,
    MO,
    MP,
    MQ,
    MR,
    MS,
    MT,
    MU,
    MV,
    MW,
    MX,
    MY,
    MZ,
    NA,
    NC,
    NE,
    NF,
    NG,
    NI,
    NL,
    NO,
    NP,
    NR,
    NU,
    NZ,
    OM,
    PA,
    PE,
    PF,
    PG,
    PH,
    PK,
    PL,
    PM,
    PN,
    PR,
    PS,
    PT,
    PW,
    PY,
    QA,
    RE,
    RO,
    RS,
    RU,
    RW,
    SA,
    SB,
    SC,
    SD,
    SE,
    SG,
    SH,
    SI,
    SJ,
    SK,
    SL,
    SM,
    SN,
    SO,
    SR,
    SS,
    ST,
    SV,
    SX,
    SY,
    SZ,
    TC,
    TD,
    TF,
    TG,
    TH,
    TJ,
    TK,
    TL,
    TM,
    TN,
    TO,
    TR,
    TT,
    TV,
    TW,
    TZ,
    UA,
    UG,
    UM,
    US,
    UY,
    UZ,
    VA,
    VC,
    VE,
    VG,
    VI,
    VN,
    VU,
    WF,
    WS,
    YE,
    YT,
    ZA,
    ZM,
    ZW,
};

/**
 * @brief Converts an Unix-like 2-letter Country code into enum dvb_country_t
 * @ingroup ancillary
 *
 * @param name	two-letter Country code.
 *
 * @return It returns the corresponding enum dvb_country_t ID. If not found,
 * 		returns COUNTRY_UNKNOWN.
 */
enum dvb_country_t dvb_country_a2_to_id(const char *name);

/**
 * @brief Converts a 3-letter Country code as used by MPEG-TS tables into
 *	  enum dvb_country_t
 * @ingroup ancillary
 *
 * @param name	three-letter Country code.
 *
 * @return It returns the corresponding enum dvb_country_t ID. If not found,
 * 		returns COUNTRY_UNKNOWN.
 */
enum dvb_country_t dvb_country_a3_to_id(const char *name);

/**
 * @brief Converts an enum dvb_country_t into Unix-like 2-letter Country code
 * @ingroup ancillary
 *
 * @param id	enum dvb_country_t ID.
 *
 * @return It returns the 2-letter country code string that corresponts to the
 *	   Country. If not found, returns NULL.
 */
const char *dvb_country_to_2letters(int id);

/**
 * @brief Converts an enum dvb_country_t into a 3-letter Country code
 * 	  as used by MPEG-TS tables
 * @ingroup ancillary
 *
 * @param id	enum dvb_country_t ID.
 *
 * @return It returns the 3-letter country code string that corresponts to the
 *	   Country. If not found, returns NULL.
 */
const char *dvb_country_to_3letters(int id);

/**
 * @brief Converts an enum dvb_country_t into a Country name
 * 	  as used by MPEG-TS tables
 * @ingroup ancillary
 *
 * @param id	enum dvb_country_t ID.
 *
 * @return It returns a string with the Country name that corresponts to the
 *	   country. If not found, returns NULL.
 */
const char *dvb_country_to_name(int id);

/**
 * @brief Guess the country code from the Unix environment variables
 * @ingroup ancillary
 *
 * @return It returns the corresponding enum dvb_country_t ID. If not found,
 * 		returns COUNTRY_UNKNOWN.
 */
enum dvb_country_t dvb_guess_user_country(void);

#ifdef __cplusplus
}
#endif

#endif
