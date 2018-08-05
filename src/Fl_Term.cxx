//
// "$Id: Fl_Term.cxx 21757 2018-06-30 13:48:10 $"
//
// Fl_Term -- A terminal simulation widget
//
// Copyright 2017-2018 by Yongchao Fan.
//
// This library is free software distributed under GNU LGPL 3.0,
// see the license at:
//
//     https://github.com/zoudaokou/flTerm/blob/master/LICENSE
//
// Please report all bugs and problems on the following page:
//
//     https://github.com/zoudaokou/flTerm/issues/new
//

#include <unistd.h>
#include <ctype.h>
#include "Fl_Term.h"
Fl_Term::Fl_Term(int X,int Y,int W,int H,const char *L) : Fl_Widget(X,Y,W,H,L) 
{
	bInsert = bAlterScreen = bAppCursor = false;
	bEscape = bGraphic = bTitle = false;
	bLogging = false;
	bMouseScroll = false;
	ESC_idx = 0;

	Fl::set_font(FL_COURIER, TERMFONT);
	color(FL_BLACK);
	textsize(14);
	strcpy(sPrompt, "> ");
	iPrompt = 2;
	iTimeOut = 30;	
	bPrompt = true;
	bLive = false;
	bEnter = bEnter1 = false;
	
	line = NULL;
	buff = attr = NULL;
	clear();
}
Fl_Term::~Fl_Term(){
	free(attr);
	free(buff);
	free(line);
};
void Fl_Term::clear()
{
	Fl::lock();
	line_size = 4096;
	buff_size = line_size*64;
	line = (int *)realloc(line, (line_size+1)*sizeof(int) );
	buff = (char *)realloc(buff, buff_size+256 );
	attr = (char *)realloc(attr, buff_size+256 );
	if ( line!=NULL ) memset(line, 0, (line_size+1)*sizeof(int) );
	if ( buff!=NULL ) memset(buff, 0, buff_size+256);
	if ( attr!=NULL ) memset(attr, 0, buff_size+256);
	cursor_y = cursor_x = 0;
	screen_y = -1;
	scroll_y = 0;
	page_up_hold = page_down_hold = 0;
	c_attr = 7;
	sel_left = 0;
	sel_right= 0;
	recv0 = 0;
	Fl::unlock();
}
void Fl_Term::more_chars()
{
	char *old_buff = buff;
	char *old_attr = attr;
	Fl::lock();
	buff = (char *)realloc(buff, buff_size*2+256);
	attr = (char *)realloc(attr, buff_size*2+256);
	if ( buff!=NULL && attr!=NULL ) {
		buff_size *= 2;
	}
	else {
		if ( attr==NULL ) free(old_attr);
		if ( buff==NULL ) free(old_buff);
		attr = buff = NULL;
		clear();
	}
	Fl::unlock();
}
void Fl_Term::more_lines()
{
	int *old_line = line;
	Fl::lock();
	line = (int *)realloc(line, (line_size*2+1)*sizeof(int));
	if ( line!=NULL ) {
		line_size *= 2; 
		for ( int i=cursor_y+1; i<line_size+1; i++ ) line[i]=0;
	}
	else {
		free(old_line);
		clear();
	}
	Fl::unlock();
}
void Fl_Term::resize( int X, int Y, int W, int H ) 
{
	Fl_Widget::resize(X,Y,W,H);
	size_y_ = (h()-8)/iFontHeight;
	size_x_ = w()/iFontWidth;
	write(NULL, 0);	//for callback to call host->send_size()
}
void Fl_Term::textsize( int pt ) 
{
	if ( pt== 0 ) {
		font_size += 2; 
		if ( font_size>16 ) font_size=12;
	}
	else 
		font_size = pt;
	fl_font(FL_COURIER, font_size);
	iFontWidth = fl_width('a'); size_x_ = w()/iFontWidth;
	iFontHeight = fl_height(); size_y_ = h()/iFontHeight;
	write(NULL, 0);	//for callback to call host->send_size()
	redraw();
}	

const int VT_attr[8]={FL_BLACK, FL_RED, FL_GREEN, FL_YELLOW, 
					 FL_BLUE, FL_MAGENTA, FL_CYAN, FL_WHITE};
void Fl_Term::draw()
{
	fl_color(color());
	fl_rectf(x(),y(),w(),h());
	fl_font(FL_COURIER, font_size);

	int sel_l = sel_left;
	int sel_r = sel_right;
	if ( sel_l>sel_r ) {
		sel_l = sel_right;
		sel_r = sel_left;
	}
	int lx = cursor_x-line[cursor_y];
	int ly = screen_y+1+scroll_y;
	if ( ly<0 ) ly=0;
	int dy = y()+iFontHeight;
	for ( int i=0; i<size_y_; i++ ) {
		int dx = x()+1;
		int j = line[ly+i];
		while( j<line[ly+i+1] ) {
			int n=j;
			while (  attr[n]==attr[j] ) {
				if ( ++n==line[ly+i+1]) break;
				if ( n==sel_r || n==sel_l ) break;
			}
			int font_color = VT_attr[(int)attr[j]%8];
			if ( j>=sel_l && j<sel_r ) {
				fl_color(selection_color());
				fl_rectf(dx, dy-iFontHeight+4, (n-j)*iFontWidth, iFontHeight);
				fl_color(fl_contrast(font_color, selection_color()));
			}
			else 
				fl_color( font_color );
				
			int k = n;
			if ( buff[k-1]==0x0a ) k--;
			fl_draw( buff+j, k-j, dx, dy );
			dx += iFontWidth*(k-j);
			j=n;
		}
		dy += iFontHeight;
	}
	if ( scroll_y || bMouseScroll) {
		fl_color(FL_DARK3);		//draw scrollbar
		fl_rectf(x()+w()-8, y(), 8, y()+h());
		fl_color(FL_RED);		//draw slider
		int slider_y = h()*(cursor_y+scroll_y)/cursor_y; 
		fl_rectf(x()+w()-8, y()+slider_y-6, 8, 12);
		fl_line( x()+w()-6, y()+slider_y-8, x()+w()-3, y()+slider_y-8 );
		fl_line( x()+w()-7, y()+slider_y-7, x()+w()-2, y()+slider_y-7 );
		fl_line( x()+w()-7, y()+slider_y+6, x()+w()-2, y()+slider_y+6 );
		fl_line( x()+w()-6, y()+slider_y+7, x()+w()-3, y()+slider_y+7 );
	}
	if ( Fl::focus()==this && active() ) {
		fl_color(FL_WHITE);		//draw a white block as cursor
		fl_rectf(x()+lx*iFontWidth+1, y()+(cursor_y-ly+1)*iFontHeight-4,8,8);
	}
}

int Fl_Term::handle( int e ) {
	switch (e) {
		case FL_FOCUS: redraw(); return 1;
		case FL_MOUSEWHEEL: 
			scroll_y += Fl::event_dy();
			if ( scroll_y<-screen_y ) scroll_y = -screen_y;
			if ( scroll_y>0 ) scroll_y = 0;	
			redraw();
			return 1;
		case FL_PUSH: if ( Fl::event_button()==FL_LEFT_MOUSE ) {
				int x=Fl::event_x()/iFontWidth; 
				int y=Fl::event_y()-Fl_Widget::y();
				if ( Fl::event_clicks()==1 ) {	//double click to select word
					y = y/iFontHeight + screen_y+scroll_y+1; 
					sel_left = line[y]+x;
					sel_right = sel_left;
					while ( --sel_left>line[y] )
						if ( !isalnum(buff[sel_left]) ) {
							sel_left++; 
							break;
						} 
					while ( ++sel_right<line[y+1])
						if ( !isalnum(buff[sel_right]) ) 
							break; 
					redraw();
					return 1;
				}
				if ( x>=size_x_-2 && scroll_y!=0 ) {	//push in scrollbar area 
					bMouseScroll = true;
					scroll_y = y*cursor_y/h()-cursor_y;
					if ( scroll_y<-screen_y ) scroll_y = -screen_y;
					redraw();
				}
				else {								//push to start draging
					y = y/iFontHeight + screen_y+scroll_y+1; 
					sel_left = line[y]+x;
					if ( sel_left>line[y+1] ) sel_left=line[y+1] ;
					sel_right = sel_left;
				}
			}
			return 1;
		case FL_DRAG: if ( Fl::event_button()==FL_LEFT_MOUSE ) {
				int x = Fl::event_x()/iFontWidth;
				int y = Fl::event_y()-Fl_Widget::y();
				if ( bMouseScroll) {
					scroll_y = y*cursor_y/h()-cursor_y;
					if ( scroll_y<-screen_y ) scroll_y = -screen_y;
					if ( scroll_y>0 ) scroll_y=0;
				}
				else {
					if ( y<0 ) {
						scroll_y += y/8;
						if ( scroll_y<-cursor_y ) scroll_y = -cursor_y;
					}
					if ( y>h() ) {
						scroll_y += (y-h())/8;
						if ( scroll_y>0 ) scroll_y=0;
					}
					y = y/iFontHeight + screen_y+scroll_y+1;
					if ( y<0 ) y=0;
					if ( y>cursor_y ) y = cursor_y; 
					sel_right = line[y]+x;
					if ( sel_right>line[y+1] ) sel_right=line[y+1];
				}
				redraw();
			}
			return 1;
		case FL_RELEASE:
			if ( Fl::event_button()==FL_LEFT_MOUSE ) {
				take_focus();		//left button drag to copy
				bMouseScroll = false;
				if ( sel_left>sel_right ) {
					int t=sel_left; sel_left=sel_right; sel_right=t;
				}
				if ( sel_left<sel_right ) 
					Fl::copy(buff+sel_left, sel_right-sel_left, 1);
			}
			else					//right click to paste
				Fl::paste(*this, 1);
			return 1; 
		case FL_DND_RELEASE:
		case FL_DND_ENTER: 
		case FL_DND_DRAG:  
		case FL_DND_LEAVE:  return 1;
		case FL_PASTE:  write(Fl::event_text(), Fl::event_length()); 
						return 1;
		case FL_KEYUP:  if ( Fl::event_state(FL_ALT)==0 ) {
				int key = Fl::event_key();
				switch ( key ) {
					case FL_Page_Up:
						page_up_hold = 0;
						break;
					case FL_Page_Down:
						page_down_hold = 0;
						break;
				}
			}
			break;
		case FL_KEYDOWN:if ( Fl::event_state(FL_ALT)==0 ) {
				int key = Fl::event_key();
				switch (key) {
				case FL_Page_Up: 
						scroll_y-=size_y_*(1<<(page_up_hold/16))-1; 
						page_up_hold++;
						if ( scroll_y<-screen_y ) scroll_y=-screen_y;
						redraw();
						break;
				case FL_Page_Down:
						scroll_y+=size_y_*(1<<(page_down_hold/16))-1; 
						page_down_hold++;
						if ( scroll_y>0 ) scroll_y = 0;
						redraw();
						break;
				case FL_Up:    write(bAppCursor?"\033OA":"\033[A",3); break;
				case FL_Down:  write(bAppCursor?"\033OB":"\033[B",3); break;
				case FL_Right: write(bAppCursor?"\033OC":"\033[C",3); break;
				case FL_Left:  write(bAppCursor?"\033OD":"\033[D",3); break;
				case FL_BackSpace: write("\177", 1); break;
				default:if ( key==FL_Enter ) {
							bEnter = true;
						}
						else {
							if ( bEnter ) {	//try to detect Prompt after each Enter 
								bEnter = false;
								bEnter1= true;
							}
						}
						write(Fl::event_text(), Fl::event_length()); 
						scroll_y = 0;
				}
				return 1;
			}
			else 
				return 0;
	}
	return Fl_Widget::handle(e);
}
void Fl_Term::append( const char *newtext, int len ){
	const char *p = newtext;

	if ( bEnter1 ) { //capture prompt for scripting after Enter key pressed
		if ( len==1 ) {	//wnen the echo is just one letter after Enter key 							
			sPrompt[0] = buff[cursor_x-2];
			sPrompt[1] = buff[cursor_x-1];
			iPrompt = 2;
		}
		bEnter1 = false;
	}
	if ( bLogging ) fwrite( newtext, 1, len, fpLogFile );
	if ( bEscape ) p = vt100_Escape( p ); 
	while ( p < newtext+len ) {
		char c=*p++;
		switch ( c ) {
		case 0x0:  break;
		case 0x07: if ( bTitle ) {
						bTitle=false;
						buff[cursor_x]=0;
						cursor_x=line[cursor_y];
						line[cursor_y+1]=cursor_x;
						//copy_label(buff+cursor_x);
					}
					break;
		case 0x08: --cursor_x; break;
		case 0x09: do { buff[cursor_x++]=' '; } 
					while ( (cursor_x-line[cursor_y])%8!=0 );
					break;
		case 0x1b: 	p = vt100_Escape( p ); break;
		case 0x0d: 	cursor_x = line[cursor_y]; break; 
		case 0x0a: 	cursor_x = line[cursor_y+1];
					if ( bAlterScreen ) { 
						if ( ++cursor_y>roll_bot+screen_y ) {
							cursor_x = line[--cursor_y];
							vt100_Escape("D");// scroll down one line
						}
						break;
					}
		default:	if ( bGraphic ) switch ( c ) {
						case 'q': c='_'; break;	//c=0xc4; break;
						case 'x':				//c=0xb3; break;
						case 't':				//c=0xc3; break;
						case 'u':				//c=0xb4; break;
						case 'm':				//c=0xc0; break;
						case 'j': c='|'; break;	//c=0xd9; break;
						case 'l':				//c=0xda; break;
						case 'k':				//c=0xbf; break;
						default: c = ' ';
					}
					if ( bInsert ) vt100_Escape("[1@");	//insert one space
					attr[cursor_x] = c_attr;
					buff[cursor_x++] = c;
					if ( line[cursor_y+1]<cursor_x ) 
						line[cursor_y+1] = cursor_x;
					if ( cursor_x>=buff_size ) more_chars(); 
					if ( c==0x0a || cursor_x-line[cursor_y]>size_x_ ) {
						line[++cursor_y] = cursor_x-(c==0x0a?0:1);
						if ( scroll_y<0 ) scroll_y--;
						if ( cursor_y==line_size ) more_lines();
						if ( line[cursor_y+1]<cursor_x ) 
							line[cursor_y+1] = cursor_x;
						if ( screen_y<cursor_y-size_y_ ) 
							screen_y = cursor_y-size_y_;
					}
		}
	}
	if ( !bPrompt )	
		if ( strncmp(sPrompt, buff+cursor_x-iPrompt, iPrompt)==0 ) 
			bPrompt=true;
	Fl::awake( this );	
}

void Fl_Term::srch( const char *word, int dirn ) 
{
	for ( int i=cursor_y+scroll_y+dirn; i>0&&i<cursor_y; i+=dirn ) {
		int len = strlen(word);
		char *p, tmp=buff[line[i+1]+len-1]; 
		buff[line[i+1]+len-1]=0;
		p = strstr(buff+line[i], word);
		buff[line[i+1]+len-1]=tmp;
		if ( p!=NULL ) {
			scroll_y = i-cursor_y;
			sel_left = p-buff;
			sel_right = sel_left+strlen(word);
			redraw();
			break;		
		}
	}
}
void Fl_Term::logg( const char *fn )
{
	if ( bLogging ) {
		fclose( fpLogFile );
		bLogging = false;
		append("\n***Log file closed***\n",23);
	}
	else {
		fpLogFile = fl_fopen( fn, "wb" );
		if ( fpLogFile != NULL ) {
			bLogging = true;
			append("\n***",4);
			append(fn, strlen(fn));
			append(" opened for logging***\n",23);
		}
	}
}
void Fl_Term::save(const char *fn)
{
	FILE *fp = fl_fopen( fn, "w" );
	if ( fp != NULL ) {
		fwrite(buff, 1, cursor_x, fp);
		fclose( fp );
		append("\n***buffer saved***\n", 20);
	}
}
const char *Fl_Term::vt100_Escape( const char *sz ) 
{
	bEscape=true; 
	while ( *sz && bEscape ){
		ESC_code[ESC_idx++] = *sz++;
		switch( ESC_code[0] ) {
		case '[':	if ( isalpha(ESC_code[ESC_idx-1]) || 
								 ESC_code[ESC_idx-1]=='@' ) {
			bEscape = false;
			int n0=1, n1=1, n2=1;
			if ( ESC_idx>1 ) {
				char *p = strchr(ESC_code,';');
				if ( p != NULL ) {
					n1 = atoi(ESC_code+1); 
					n2 = atoi(p+1);
				}
				else if ( isdigit(ESC_code[1]) ) 
						n0 = atoi(ESC_code+1);
			}
			int x;
			switch ( ESC_code[ESC_idx-1] ) {
			case 'A': //cursor up n0 lines
				x = cursor_x-line[cursor_y]; 
				cursor_y-=n0; 
				cursor_x = line[cursor_y]+x; 
				break;
			case 'B': //cursor down n0 lines
				x = cursor_x-line[cursor_y]; 
				cursor_y+=n0; 
				cursor_x = line[cursor_y]+x; 
				break;
			case 'C': //cursor right n0 positions
				cursor_x+=n0; break; 
			case 'D': //cursor left n0 positions
				cursor_x-=n0; break;
			case 'G': //cursor to n0th position from left
				cursor_x = line[cursor_y]+n0-1; break;
			case 'H': //cursor to line n1, postion n2
				if ( n1>size_y_ ) n1 = size_y_;
				if ( n2>size_x_ ) n2 = size_x_;
				cursor_y = screen_y+n1; 
				cursor_x = line[cursor_y]+n2-1;
				break;
			case 'J': {//[J kill till end, 1J begining, 2J entire screen
				int i=0, j=size_y_;
				if ( ESC_code[ESC_idx-2]=='[' ) {
					i=cursor_y-screen_y;
					memset( buff+cursor_x, ' ', line[cursor_y+1]-cursor_x );
					memset( attr+cursor_x,   0, line[cursor_y+1]-cursor_x );
				}
				else {
					cursor_y = screen_y+1;
					cursor_x = line[cursor_y];
					if ( n0==1 ) j = cursor_y-screen_y-1;
				}
				char empty[256];
				memset(empty, ' ', 256);
				for ( ; i<j; i++ ) append(empty, size_x_);
				}
				break;
			case 'K': {//[K kill till end, 1K begining, 2K entire line
				int i=line[cursor_y], j=line[cursor_y+1];
				if ( ESC_code[ESC_idx-2]=='[' ) i = cursor_x;
				else if ( n0==1 ) j = cursor_x;
				memset(buff+i, ' ', j-i);
				memset(attr+i,   0, j-i);
				}
				break;
			case 'L': //insert lines
				for ( int i=roll_bot; i>cursor_y-screen_y; i-- ) {
					memcpy( buff+line[screen_y+i], 
							buff+line[screen_y+i-n0], size_x_ );
					memcpy( attr+line[screen_y+i], 
							attr+line[screen_y+i-n0], size_x_ );
				}
				for ( int i=0; i<n0; i++ ) {
					memset(buff+line[cursor_y+i], ' ', size_x_);
					memset(attr+line[cursor_y+i],   0, size_x_);
				}
				break; 
			case 'M': //delete lines
				for ( int i=cursor_y-screen_y; i<=roll_bot-n0; i++ ) {
					memcpy( buff+line[screen_y+i], 
							buff+line[screen_y+i+n0], size_x_);
					memcpy( attr+line[screen_y+i], 
							attr+line[screen_y+i+n0], size_x_);
				}
				for ( int i=0; i<n0; i++ ) {
					memset(buff+line[screen_y+roll_bot+1-i], ' ', size_x_);
					memset(attr+line[screen_y+roll_bot+1-i],   0, size_x_);
				}
				break;
			case 'P': //remove n0 letters
				for ( int i=cursor_x; i<line[cursor_y+1]-n0; i++ )
					{ buff[i]=buff[i+n0]; attr[i]=attr[i+n0]; }
				line[cursor_y+1]-=n0;
				break;
			case '@': //insert n0 spaces
				for ( int i=line[cursor_y+1]; i>=cursor_x; i-- ) 
					{ buff[i+n0]=buff[i]; attr[i+n0]=attr[i]; }
				for ( int i=0; i<n0; i++ ) 
					{ buff[cursor_x+i]=' '; attr[cursor_x+i]=0; }
				line[cursor_y+1]+=n0;
				break;
			case 'h': 
			case 'l': 
				if ( ESC_code[1]=='4' ) {
					if ( ESC_code[2]=='h' ) bInsert=true;
					if ( ESC_code[2]=='l' ) bInsert=false;
				}
				if ( ESC_code[1]=='?' ) {
					n0 = atoi(ESC_code+2);
					if ( n0==1 ) {// ?1h app cursor key, ?1l exit app cursor key
						bAlterScreen = bAppCursor = (ESC_code[ESC_idx-1]=='h');
						roll_top = 1; roll_bot = size_y_;
					}
					if ( n0==1049 ) { 	//?1049h alternate screen, 
						if ( ESC_code[ESC_idx-1]=='h' ) {
							save_y = cursor_y;
							char empty[256];
							memset(empty, ' ', 256);
							for ( int i=0; i<size_y_; i++ ) 
								append(empty, size_x_);
							bAlterScreen = true;
						}
						else {			//?1049l exit alternate screen
							cursor_y = save_y; 
							cursor_x = line[cursor_y];
							for ( int i=1; i<=size_y_; i++ ) 
								line[cursor_y+i] = 0;
							screen_y -= size_y_-1;
							if ( screen_y<0 ) screen_y=0;
							bAlterScreen = false; 
						}
					}
				}
				break;
			case 'm': //text style, color attributes 
				if ( n0<10 ) c_attr = 7;
				if ( int(n0/10)==3 ) c_attr = n0%10;
				if ( n1==1 && n2/10==3 ) c_attr = n2%10; 
				break;	
			case 'r': roll_top=n1; roll_bot=n2; break;
				}
			}
			break;
		case 'D': // scroll down one line
			for ( int i=roll_top; i<roll_bot; i++ ) {
				memcpy(buff+line[screen_y+i], buff+line[screen_y+i+1], size_x_);
				memcpy(attr+line[screen_y+i], attr+line[screen_y+i+1], size_x_);
			}
			memset(buff+line[screen_y+roll_bot], ' ', size_x_);
			memset(attr+line[screen_y+roll_bot],   0, size_x_);
			bEscape = false;
			break;
		case 'M': // scroll up one line
			for ( int i=roll_bot; i>roll_top; i-- ) {
				memcpy(buff+line[screen_y+i], buff+line[screen_y+i-1], size_x_);
				memcpy(attr+line[screen_y+i], attr+line[screen_y+i-1], size_x_);
			}
			memset(buff+line[screen_y+roll_top], ' ', size_x_);
			memset(attr+line[screen_y+roll_top],   0, size_x_);
			bEscape = false;
			break; 
		case ']':if ( ESC_code[ESC_idx-1]==';' ) {
					if ( ESC_code[1]=='0' ) bTitle = true;
					bEscape = false;
				 }
				 break;
		case '(':if ( (ESC_code[1]=='B'||ESC_code[1]=='0') ) {
					bGraphic = (ESC_code[1]=='0');
					bEscape = false; 
				 }
				 break;
		default: bEscape = false;
		}
		if ( ESC_idx==20 ) bEscape = false;
		if ( !bEscape ) { ESC_idx=0; memset(ESC_code, 0, 20); }
	}
	return sz;
}

int Fl_Term::waitfor(const char *word)
{
	char *p = buff+recv0;
	bWait = true;
	for ( int i=0; i<iTimeOut*10&&bWait; i++ ) {
		buff[cursor_x]=0;
		if ( strstr(p, word)!=NULL ) return 1;
		sleep(1);
	}
	return 0;
}
void Fl_Term::prompt(char *p)
{
	strncpy(sPrompt, p, 31);
	iPrompt = strlen(sPrompt);
}
void Fl_Term::mark_prompt()
{
	bPrompt = false; 
}
void Fl_Term::wait_prompt()
{
	int oldlen = cursor_x;
	for ( int i=0; i<iTimeOut && !bPrompt; i++ ) {
		sleep(1);
		if ( cursor_x>oldlen ) { i=0; oldlen=cursor_x; }
	}	
	bPrompt = true;
}	
int Fl_Term::command( const char *cmd, char **preply )
{
	mark_prompt();
	write(cmd, strlen(cmd)); write("\015", 1);
	recv0 = cursor_x;
	wait_prompt();
	if ( preply!=NULL ) *preply = buff+recv0;	
	return cursor_x-recv0;
}
void Fl_Term::putxml(const char *msg, int len)
{
	int indent=0, previousIsOpen=true;
	const char *p=msg, *q;
	const char spaces[256]="\r\n                                               \
                                                                              ";
	while ( p<msg+len ) {
		while (*p==0x0d || *p==0x0a || *p=='\t' || *p==' ' ) p++;
		if ( *p=='<' ) { //tag
			if ( p[1]=='/' ) {
				if ( !previousIsOpen ) {
					indent -= 2;
					append(spaces, indent);
				}
				previousIsOpen = false;
			}
			else {
				if ( previousIsOpen ) indent+=2;
				append(spaces, indent);
				previousIsOpen = true;
			}
			q = strchr(p, '>');
			if ( q!=NULL ) {
				append("\033[32m",5);
				const char *r = strchr(p, ' ');
				if ( r!=NULL && r<q ) {
					append(p, r-p);
					append("\033[35m",5);
					append(r, q-r);
				}
				else
					append(p, q-p);
				append("\033[32m>",6);
				if ( q[-1]=='/' ) previousIsOpen = false;
				p = q+1;
			}
			else 
				break;
		}
		else {			//data
			q = strchr(p, '<');
			if ( q!=NULL ) {
				append("\033[33m",5);
				append(p, q-p);
				p = q;
			}
			else { //not xml or incomplete xml
				append(p, strlen(p));
				break;
			}
		}	
	}
	append("\033[37m",5);
}
