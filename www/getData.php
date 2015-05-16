<?php
      
  $day = $_POST["den"];
  
    $files = glob("EPG/*");
    foreach($files as $file) {
        $xml=simplexml_load_file($file); 
        echo "<td>";
        echo "<span class=\"nazev\">" . $xml->channel->{'display-name'} . "</span></br>";
        echo "_____________________________________</br>";
        
        foreach($xml->programme as $program) { 
          $start = $program['start'];             
          $stop = $program['stop'];
       
          $dt = new DateTime("@$start");

	  if($day == $dt->format('d-m-Y') || $day == "all"){   
          echo $dt->format('H:i d-m-Y') . " - ";
          $dt = new DateTime("@$stop");
          echo $dt->format('H:i d-m-Y') . "</br>";
          echo "<b>" . $program->title . "</b></br>";
          if(strlen($program->desc) > 0 ){
           echo $program->desc . "</br>";
          }
          echo "_____________________________________</br>";

          }
        }         
        echo "</td>";
    }     
?>