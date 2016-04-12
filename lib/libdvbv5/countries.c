/*
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>
#include "libdvbv5/countries.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

struct cCountry {
	enum dvb_country_t id;
	const char *alpha2_name;
	const char *alpha3_name;
	const char *short_name;
};

/* 
 * constants from ISO 3166-1.
 * sorted alphabetically by two-letter(alpha-2) country name and
 * in the same order with enum dvb_country_t.
 */
static const struct cCountry country_list[] = {
	/* id, alpha-2, alpha-3, short name */

	{ COUNTRY_UNKNOWN, "", "", "(Unknown Country)" },

	{ AD, "AD", "AND", "Andorra" },
	{ AE, "AE", "ARE", "United Arab Emirates" },
	{ AF, "AF", "AFG", "Afghanistan" },
	{ AG, "AG", "ATG", "Antigua and Barbuda" },
	{ AI, "AI", "AIA", "Anguilla" },
	{ AL, "AL", "ALB", "Albania" },
	{ AM, "AM", "ARM", "Armenia" },
	{ AO, "AO", "AGO", "Angola" },
	{ AQ, "AQ", "ATA", "Antarctica" },
	{ AR, "AR", "ARG", "Argentina" },
	{ AS, "AS", "ASM", "American Samoa" },
	{ AT, "AT", "AUT", "Austria" },
	{ AU, "AU", "AUS", "Australia" },
	{ AW, "AW", "ABW", "Aruba" },
	{ AX, "AX", "ALA", "Aland Islands" },
	{ AZ, "AZ", "AZE", "Azerbaijan" },
	{ BA, "BA", "BIH", "Bosnia and Herzegovina" },
	{ BB, "BB", "BRB", "Barbados" },
	{ BD, "BD", "BGD", "Bangladesh" },
	{ BE, "BE", "BEL", "Belgium" },
	{ BF, "BF", "BFA", "Burkina Faso" },
	{ BG, "BG", "BGR", "Bulgaria" },
	{ BH, "BH", "BHR", "Bahrain" },
	{ BI, "BI", "BDI", "Burundi" },
	{ BJ, "BJ", "BEN", "Benin" },
	{ BL, "BL", "534", "Saint Barthelemy" },
	{ BM, "BM", "BMU", "Bermuda" },
	{ BN, "BN", "BRN", "Brunei Darussalam" },
	{ BO, "BO", "BOL", "Plurinational State of Bolivia" },
	{ BQ, "BQ", "BES", "Bonaire, Saint Eustatius and Saba" },
	{ BR, "BR", "BRA", "Brazil" },
	{ BS, "BS", "BHS", "Bahamas" },
	{ BT, "BT", "BTN", "Bhutan" },
	{ BV, "BV", "BVT", "Bouvet Island" },
	{ BW, "BW", "BWA", "Botswana" },
	{ BY, "BY", "BLR", "Belarus" },
	{ BZ, "BZ", "BLZ", "Belize" },
	{ CA, "CA", "CAN", "Canada" },
	{ CC, "CC", "CCK", "Cocos (Keeling) Islands" },
	{ CD, "CD", "COD", "The Democratic Republic of the Congo" },
	{ CF, "CF", "CAF", "Central African Republic" },
	{ CG, "CG", "COG", "Congo" },
	{ CH, "CH", "CHE", "Switzerland" },
	{ CI, "CI", "CIV", "Cote d'Ivoire" },
	{ CK, "CK", "COK", "Cook Islands" },
	{ CL, "CL", "CHL", "Chile" },
	{ CM, "CM", "CMR", "Cameroon" },
	{ CN, "CN", "CHN", "China" },
	{ CO, "CO", "COL", "Colombia" },
	{ CR, "CR", "CRI", "Costa Rica" },
	{ CU, "CU", "CUB", "Cuba" },
	{ CV, "CV", "CPV", "Cape Verde" },
	{ CW, "CW", "CUW", "Curacao" },
	{ CX, "CX", "CXR", "Christmas Island" },
	{ CY, "CY", "CYP", "Cyprus" },
	{ CZ, "CZ", "CZE", "Czech Republic" },
	{ DE, "DE", "DEU", "Germany" },
	{ DJ, "DJ", "DJI", "Djibouti" },
	{ DK, "DK", "DNK", "Denmark" },
	{ DM, "DM", "DMA", "Dominica" },
	{ DO, "DO", "DOM", "Dominican Republic" },
	{ DZ, "DZ", "DZA", "Algeria" },
	{ EC, "EC", "ECU", "Ecuador" },
	{ EE, "EE", "EST", "Estonia" },
	{ EG, "EG", "EGY", "Egypt" },
	{ EH, "EH", "ESH", "Western Sahara" },
	{ ER, "ER", "ERI", "Eritrea" },
	{ ES, "ES", "ESP", "Spain" },
	{ ET, "ET", "ETH", "Ethiopia" },
	{ FI, "FI", "FIN", "Finland" },
	{ FJ, "FJ", "FJI", "Fiji" },
	{ FK, "FK", "FLK", "Falkland Islands (Malvinas)" },
	{ FM, "FM", "FSM", "Federated States of Micronesia" },
	{ FO, "FO", "534", "Faroe Islands" },
	{ FR, "FR", "FRA", "France" },
	{ GA, "GA", "GAB", "Gabon" },
	{ GB, "GB", "GBR", "United Kingdom" },
	{ GD, "GD", "GRD", "Grenada" },
	{ GE, "GE", "GEO", "Georgia" },
	{ GF, "GF", "GUF", "French Guiana" },
	{ GG, "GG", "GGY", "Guernsey" },
	{ GH, "GH", "GHA", "Ghana" },
	{ GI, "GI", "GIB", "Gibraltar" },
	{ GL, "GL", "GRL", "Greenland" },
	{ GM, "GM", "GMB", "Gambia" },
	{ GN, "GN", "GIN", "Guinea" },
	{ GP, "GP", "GLP", "Guadeloupe" },
	{ GQ, "GQ", "GNQ", "Equatorial Guinea" },
	{ GR, "GR", "GRC", "Greece" },
	{ GS, "GS", "SGS", "South Georgia and the South Sandwich Islands" },
	{ GT, "GT", "GTM", "Guatemala" },
	{ GU, "GU", "GUM", "Guam" },
	{ GW, "GW", "GNB", "Guinea-Bissau" },
	{ GY, "GY", "GUY", "Guyana" },
	{ HK, "HK", "HKG", "Hong Kong" },
	{ HM, "HM", "HMD", "Heard Island and McDonald Islands" },
	{ HN, "HN", "HND", "Honduras" },
	{ HR, "HR", "HRV", "Croatia" },
	{ HT, "HT", "HTI", "Haiti" },
	{ HU, "HU", "HUN", "Hungary" },
	{ ID, "ID", "IDN", "Indonesia" },
	{ IE, "IE", "IRL", "Ireland" },
	{ IL, "IL", "ISR", "Israel" },
	{ IM, "IM", "IMN", "Isle of Man" },
	{ IN, "IN", "IND", "India" },
	{ IO, "IO", "IOT", "British Indian Ocean Territory" },
	{ IQ, "IQ", "IRQ", "Iraq" },
	{ IR, "IR", "IRN", "Islamic Republic of Iran" },
	{ IS, "IS", "ISL", "Iceland" },
	{ IT, "IT", "ITA", "Italy" },
	{ JE, "JE", "JEY", "Jersey" },
	{ JM, "JM", "JAM", "Jamaica" },
	{ JO, "JO", "JOR", "Jordan" },
	{ JP, "JP", "JPN", "Japan" },
	{ KE, "KE", "KEN", "Kenya" },
	{ KG, "KG", "KGZ", "Kyrgyzstan" },
	{ KH, "KH", "KHM", "Cambodia" },
	{ KI, "KI", "KIR", "Kiribati" },
	{ KM, "KM", "COM", "Comoros" },
	{ KN, "KN", "KNA", "Saint Kitts and Nevis" },
	{ KP, "KP", "PRK", "Democratic People's Republic of Korea" },
	{ KR, "KR", "KOR", "Republic of Korea" },
	{ KW, "KW", "KWT", "Kuwait" },
	{ KY, "KY", "CYM", "Cayman Islands" },
	{ KZ, "KZ", "KAZ", "Kazakhstan" },
	{ LA, "LA", "LAO", "Lao People's Democratic Republic" },
	{ LB, "LB", "LBN", "Lebanon" },
	{ LC, "LC", "LCA", "Saint Lucia" },
	{ LI, "LI", "LIE", "Liechtenstein" },
	{ LK, "LK", "LKA", "Sri Lanka" },
	{ LR, "LR", "LBR", "Liberia" },
	{ LS, "LS", "LSO", "Lesotho" },
	{ LT, "LT", "LTU", "Lithuania" },
	{ LU, "LU", "LUX", "Luxembourg" },
	{ LV, "LV", "LVA", "Latvia" },
	{ LY, "LY", "LBY", "Libyan Arab Jamahiriya" },
	{ MA, "MA", "MAR", "Morocco" },
	{ MC, "MC", "MCO", "Monaco" },
	{ MD, "MD", "MDA", "Republic of Moldova" },
	{ ME, "ME", "MNE", "Montenegro" },
	{ MF, "MF", "MAF", "Saint Martin (French part)" },
	{ MG, "MG", "MDG", "Madagascar" },
	{ MH, "MH", "MHL", "Marshall Islands" },
	{ MK, "MK", "MKD", "The Former Yugoslav Republic of Macedonia" },
	{ ML, "ML", "MLI", "Mali" },
	{ MM, "MM", "MMR", "Myanmar" },
	{ MN, "MN", "MNG", "Mongolia" },
	{ MO, "MO", "MAC", "Macao" },
	{ MP, "MP", "MNP", "Northern Mariana Islands" },
	{ MQ, "MQ", "MTQ", "Martinique" },
	{ MR, "MR", "MRT", "Mauritania" },
	{ MS, "MS", "MSR", "Montserrat" },
	{ MT, "MT", "MLT", "Malta" },
	{ MU, "MU", "MUS", "Mauritius" },
	{ MV, "MV", "MDV", "Maldives" },
	{ MW, "MW", "MWI", "Malawi" },
	{ MX, "MX", "MEX", "Mexico" },
	{ MY, "MY", "MYS", "Malaysia" },
	{ MZ, "MZ", "MOZ", "Mozambique" },
	{ NA, "NA", "NAM", "Namibia" },
	{ NC, "NC", "NCL", "New Caledonia" },
	{ NE, "NE", "NER", "Niger" },
	{ NF, "NF", "NFK", "Norfolk Island" },
	{ NG, "NG", "NGA", "Nigeria" },
	{ NI, "NI", "NIC", "Nicaragua" },
	{ NL, "NL", "NLD", "Netherlands" },
	{ NO, "NO", "NOR", "Norway" },
	{ NP, "NP", "NPL", "Nepal" },
	{ NR, "NR", "NRU", "Nauru" },
	{ NU, "NU", "NIU", "Niue" },
	{ NZ, "NZ", "NZL", "New Zealand" },
	{ OM, "OM", "OMN", "Oman" },
	{ PA, "PA", "PAN", "Panama" },
	{ PE, "PE", "PER", "Peru" },
	{ PF, "PF", "PYF", "French Polynesia" },
	{ PG, "PG", "PNG", "Papua New Guinea" },
	{ PH, "PH", "PHL", "Philippines" },
	{ PK, "PK", "PAK", "Pakistan" },
	{ PL, "PL", "POL", "Poland" },
	{ PM, "PM", "SPM", "Saint Pierre and Miquelon" },
	{ PN, "PN", "PCN", "Pitcairn" },
	{ PR, "PR", "PRI", "Puerto Rico" },
	{ PS, "PS", "PSE", "Occupied Palestinian Territory" },
	{ PT, "PT", "PRT", "Portugal" },
	{ PW, "PW", "PLW", "Palau" },
	{ PY, "PY", "PRY", "Paraguay" },
	{ QA, "QA", "QAT", "Qatar" },
	{ RE, "RE", "REU", "Reunion" },
	{ RO, "RO", "ROU", "Romania" },
	{ RS, "RS", "SRB", "Serbia" },
	{ RU, "RU", "RUS", "Russian Federation" },
	{ RW, "RW", "RWA", "Rwanda" },
	{ SA, "SA", "SAU", "Saudi Arabia" },
	{ SB, "SB", "SLB", "Solomon Islands" },
	{ SC, "SC", "SYC", "Seychelles" },
	{ SD, "SD", "SDN", "Sudan" },
	{ SE, "SE", "SWE", "Sweden" },
	{ SG, "SG", "SGP", "Singapore" },
	{ SH, "SH", "SHN", "Saint Helena, Ascension and Tristan da Cunha" },
	{ SI, "SI", "SVN", "Slovenia" },
	{ SJ, "SJ", "534", "Svalbard and Jan Mayen" },
	{ SK, "SK", "SVK", "Slovakia" },
	{ SL, "SL", "SLE", "Sierra Leone" },
	{ SM, "SM", "SMR", "San Marino" },
	{ SN, "SN", "SEN", "Senegal" },
	{ SO, "SO", "SOM", "Somalia" },
	{ SR, "SR", "SUR", "Suriname" },
	{ SS, "SS", "SSD", "South Sudan" },
	{ ST, "ST", "STP", "Sao Tome and Principe" },
	{ SV, "SV", "SLV", "El Salvador" },
	{ SX, "SX", "SXM", "Sint Maarten (Dutch part)" },
	{ SY, "SY", "SYR", "Syrian Arab Republic" },
	{ SZ, "SZ", "SWZ", "Swaziland" },
	{ TC, "TC", "TCA", "Turks and Caicos Islands" },
	{ TD, "TD", "TCD", "Chad" },
	{ TF, "TF", "ATF", "French Southern Territories" },
	{ TG, "TG", "TGO", "Togo" },
	{ TH, "TH", "THA", "Thailand" },
	{ TJ, "TJ", "TJK", "Tajikistan" },
	{ TK, "TK", "TKL", "Tokelau" },
	{ TL, "TL", "TLS", "Timor-Leste" },
	{ TM, "TM", "TKM", "Turkmenistan" },
	{ TN, "TN", "TUN", "Tunisia" },
	{ TO, "TO", "TON", "Tonga" },
	{ TR, "TR", "TUR", "Turkey" },
	{ TT, "TT", "TTO", "Trinidad and Tobago" },
	{ TV, "TV", "TUV", "Tuvalu" },
	{ TW, "TW", "TWN", "Taiwan, Province of China" },
	{ TZ, "TZ", "TZA", "United Republic of Tanzania" },
	{ UA, "UA", "UKR", "Ukraine" },
	{ UG, "UG", "UGA", "Uganda" },
	{ UM, "UM", "UMI", "United States Minor Outlying Islands" },
	{ US, "US", "USA", "United States" },
	{ UY, "UY", "URY", "Uruguay" },
	{ UZ, "UZ", "UZB", "Uzbekistan" },
	{ VA, "VA", "VAT", "Holy See (Vatican City State)" },
	{ VC, "VC", "VCT", "Saint Vincent and The Grenadines" },
	{ VE, "VE", "VEN", "Bolivarian Republic of Venezuela" },
	{ VG, "VG", "VGB", "British Virgin Islands" },
	{ VI, "VI", "VIR", "U.S. Virgin Islands" },
	{ VN, "VN", "VNM", "Viet Nam" },
	{ VU, "VU", "VUT", "Vanuatu" },
	{ WF, "WF", "WLF", "Wallis and Futuna" },
	{ WS, "WS", "WSM", "Samoa" },
	{ YE, "YE", "YEM", "Yemen" },
	{ YT, "YT", "MYT", "Mayotte" },
	{ ZA, "ZA", "ZAF", "South Africa" },
	{ ZM, "ZM", "ZMB", "Zambia" },
	{ ZW, "ZW", "ZWE", "Zimbabwe" },
};

#define COUNTRY_COUNT (sizeof(country_list)/sizeof(struct cCountry))

static int cmp_alpha2(const void *key, const void *b)
{
	const struct cCountry *elem = b;

	return strcasecmp(key, elem->alpha2_name);
}

static int cmp_alpha3(const void *key, const void *b)
{
	const struct cCountry *elem = b;

	return strcasecmp(key, elem->alpha3_name);
}


/* convert ISO 3166-1 two-letter constant (alpha-2)
 * to index number
 * return 0(COUNTRY_UNKNOWN) if not found.
 */
enum dvb_country_t dvb_country_a2_to_id(const char *name)
{
	const struct cCountry *p;

	p = bsearch(name, country_list,
			COUNTRY_COUNT, sizeof(country_list[0]), cmp_alpha2);
	return p ? p->id : COUNTRY_UNKNOWN;
}

/* convert ISO 3166-1 three-letter constant (alpha-3)
 * to index number
 * return 0(COUNTRY_UNKNOWN) if not found.
 */
enum dvb_country_t dvb_country_a3_to_id(const char *name)
{
	const struct cCountry *p;

	p = bsearch(name, country_list,
			COUNTRY_COUNT, sizeof(country_list[0]), cmp_alpha3);
	return p ? p->id : COUNTRY_UNKNOWN;
}


/* convert index number
 * to ISO 3166-1 two-letter constant
 * return NULL if not found.
 */
const char *dvb_country_to_2letters(int idx)
{
	return (idx >= 1 && idx < COUNTRY_COUNT)
			 ? country_list[idx].alpha2_name : NULL;
}

/* convert index number
 * to ISO 3166-1 three-letter constant
 * return NULL if not found.
 */
const char *dvb_country_to_3letters(int idx)
{
	return (idx >= 1 && idx < COUNTRY_COUNT)
			 ? country_list[idx].alpha3_name : NULL;
}

/* convert index number
 * to country name
 * return NULL if not found.
 */
const char *dvb_country_to_name(int idx)
{
	return (idx >= 1 && idx < COUNTRY_COUNT)
			 ? country_list[idx].short_name : NULL;
}

#ifndef HAVE_SECURE_GETENV
#  ifdef HAVE___SECURE_GETENV
#    define secure_getenv __secure_getenv
#  else
#    define secure_getenv getenv
#  endif
#endif

#define MIN(X,Y) (X < Y ? X : Y)

static const char * cats[] = {
 "LC_ALL", "LC_CTYPE", "LC_COLLATE", "LC_MESSAGES", "LANG"
};

enum dvb_country_t dvb_guess_user_country(void)
{
	char * buf, * pch, * pbuf;
	unsigned cat;
	enum dvb_country_t id = COUNTRY_UNKNOWN;

	for (cat = 0; cat < sizeof(cats)/sizeof(cats[0]); cat++) {

		// the returned char * should be "C", "POSIX" or something valid.
		// If valid, we can only *guess* which format is returned.
		// Assume here something like "de_DE.iso8859-1@euro" or "de_DE.utf-8"
		buf = secure_getenv(cats[cat]);
		if (! buf || strlen(buf) < 2)
			continue;

		if (! strncmp(buf, "POSIX", MIN(strlen(buf), 5)) ||
		    ! (strncmp(buf, "en", MIN(strlen(buf), 2)) && !isalpha(buf[2])) )
			continue;

		buf = strdup(buf);
		pbuf= buf;

		// assuming 'language_country.encoding@variant'

		// country after '_', if given
		if ((pch = strchr(buf, '_')))
			pbuf = pch + 1;

		// remove all after '@', including '@'
		if ((pch = strchr(pbuf, '@')))
			*pch = 0;

		// remove all after '.', including '.'
		if ((pch = strchr(pbuf, '.')))
			*pch = 0;

		if (strlen(pbuf) == 2)
			id = dvb_country_a2_to_id(pbuf);
		free(buf);
		if (id != COUNTRY_UNKNOWN)
			return id;
	}

	return COUNTRY_UNKNOWN;
}
