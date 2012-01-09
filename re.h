/* vim: set noexpandtab:tabstop=8 */
enum { RE_LIT_=1, RE_DOT_, RE_BRA_, RE_CBRA_, RE_BRK_, RE_BKSP_, RE_STAR_=8, RE_Q_=16 };
static _re_map_[]={1,2,4,8,16,32,64,128};

static wchar_t*
re_comp(wchar_t *out, wchar_t *re) {
	wchar_t	*spec=L".*[?\\", *esc=L"n\nr\rt\t";
	wchar_t	*o=out, *prev=0, *c;
	int	span, i;
	for ( ; *re; re++)
		if (span=(wcscspn(re,spec) & 127)) {
			*(prev=o++)=RE_LIT_;
			*o++=span;
			wmemcpy(o,re,span);
			re+=span-1, o+=span;
		} else if (*re=='.')
			*(prev=o++)=RE_DOT_;
		else if (*re=='[') {
			*(prev=o++)=re[1]=='^'
				? (re++,RE_CBRA_)
				: RE_BRA_;
			memset(o,0,32);
			for (re++; *re && *re!=']'; re++)
				if (re[1]=='-' && (re+=2))
					for (i=re[-2]; i<=*re; i++)
						o[i/8] |= _re_map_[i&7];
				else
					o[*re/8] |= _re_map_[*re&7];
			o+=32;
		} else if (*re=='*' && prev)
			*prev|=RE_STAR_;
		else if (*re=='?' && prev)
			*prev|=RE_Q_;
		else if (*re=='\\' && re[1]=='b')
			*(prev=o++)=RE_BRK_, re++;
		else if (*re=='\\' && re[1]=='~')
			*(prev=o++)=RE_BKSP_, re++;
		else if (*re=='\\') {
			*(prev=o++)=RE_LIT_;
			*o++=1;
			for (c=esc, re++; *c && *c!=*re; c+=2);
			*o++=*c? *c: *re;
		} else {
			*(prev=o++)=RE_LIT_;
			*o++=1;
			*o++=*re;
		}
			
	*o++=0;
	return out;
}

static
re_run(wchar_t *txt, wchar_t *m) {
	wchar_t	*org=txt;
	int	c;
	for ( ; *m; m++)
		switch (*m & 7) {
		case RE_LIT_:
			if (*m & RE_STAR_)
				while (!wcsncmp(txt,m+2,m[1]))
					txt+=m[1];
			else if (*m & RE_Q_) {
				if (!wcsncmp(txt,m+2,m[1]))
					txt+=m[1];
			} else {
				if (wcsncmp(txt,m+2,m[1]))
					return -1;
				txt+=m[1];
			}
			m+=m[1]+1;
			break;
		case RE_BRA_:
		case RE_CBRA_:
			c=(*m & 7)==RE_CBRA_;
			if (*m & RE_STAR_)
				while (*txt && c==!(m[1+*txt/8] & _re_map_[*txt&7]))
					txt++;
			else if (*m & RE_Q_) {
				if (*txt && c==!(m[1+*txt/8] & _re_map_[*txt&7]))
					txt++;
			} else {
				if (!*txt || c==!!(m[1+*txt/8] & _re_map_[*txt&7]))
					return -1;
				txt++;
			}
			m+=32;
			break;
		case RE_DOT_:
			if (*m & RE_STAR_)
				while (*txt && txt++);
			else if (*m & RE_Q_)
				*txt && txt++;
			else {
				if (!*txt)
					return -1;
				txt++;
			}
			break;
		case RE_BRK_:
			if (!(*m & RE_STAR_)
			&& !(*m & RE_Q_)
			&& !brktbl[*txt&0xffff])
				return -1;
			break;
		case RE_BKSP_:
			if (*m & RE_STAR_)
				txt = org;
			else if (org < txt)
				txt--;
			break;
		}
	return txt-org;
}
